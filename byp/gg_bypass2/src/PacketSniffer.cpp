#include "include/sdk.h"
#include <cstdio>
#include <cstring>
#include <map>
#include <vector>

#include "include/PacketSniffer.h"
#include "include/Config.h"
#include "include/Logger.h"
#include "include/GameMemory.h"
#include "include/SimpleModulusCipher.h"
#include "include/ViewportMap.h"
#include "include/SkillMap.h"

#pragma comment(lib, "ws2_32.lib")

// ---- Stack trace manual via EBP walk (x86) ----
// O EBP é capturado no caller (HookSend) e passado aqui.
static void LogStackTrace(const char* prefix, uintptr_t* pEbp) {
    char buf[2048];
    int pos = snprintf(buf, sizeof(buf), "%s:", prefix);

    int depth = 0;
    while (pEbp && depth < 32 && pos < (int)sizeof(buf) - 100) {
        // Proteção contra EBP inválido
        if ((uintptr_t)pEbp < 0x10000 || ((uintptr_t)pEbp & 3)) break;
        __try {
            uintptr_t retAddr = pEbp[1];
            if (retAddr == 0 || retAddr < 0x10000) break;

            MEMORY_BASIC_INFORMATION mbi;
            if (VirtualQuery((void*)retAddr, &mbi, sizeof(mbi)) && mbi.AllocationBase) {
                HMODULE hMod = (HMODULE)mbi.AllocationBase;
                char modPath[256];
                if (GetModuleFileNameA(hMod, modPath, sizeof(modPath))) {
                    const char* basename = strrchr(modPath, '\\');
                    basename = basename ? basename + 1 : modPath;
                    uintptr_t offset = retAddr - (uintptr_t)hMod;
                    pos += snprintf(buf + pos, sizeof(buf) - pos, " %s+0x%X", basename, (unsigned)offset);
                } else {
                    pos += snprintf(buf + pos, sizeof(buf) - pos, " 0x%p", (void*)retAddr);
                }
            } else {
                pos += snprintf(buf + pos, sizeof(buf) - pos, " 0x%p", (void*)retAddr);
            }

            pEbp = (uintptr_t*)pEbp[0];
            depth++;
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            break;
        }
    }

    Logger::Log("PACKETSNIFFER", "%s", buf);
}

namespace PacketSniffer {

static SOCKET g_TcpSocket = INVALID_SOCKET;
static CRITICAL_SECTION g_TcpLock;
static HANDLE g_ConnectThread = NULL;
static HANDLE g_RecvThread = NULL;
static bool g_Running = false;
static bool g_Initialized = false;

// Socket do game server do JOGO (conectado ao MuProxy2 → relay → GS real)
// Usamos este socket para injeção porque é a MESMA conexão TCP autenticada
static SOCKET g_GameServerSocket = INVALID_SOCKET;

// SkillPacketType enum definido em PacketSniffer.h

// ---- Fila thread-safe para skill attacks (C3) ----
// AttackModule enfileira o plaintext; HookSend processa na thread do jogo
// chamando SimpleModulus::Encrypt (0x998420) no contexto seguro.
// std::vector cresce dinamicamente — sem limite de monstros.
struct SkillPacketEntry {
    uint8_t data[8];   // plaintext (max 8 bytes)
    int len;            // tamanho real
    SkillPacketType type;  // struct ou builder
    uint16_t skillId;      // skill ID (para BUILDER path)
    uint16_t attackId;     // target index — RAW entity index (sem XOR 0xEFF5!)
                            // O servidor MuEmu lê: bIndex = MAKE_NUMBERW(indexH, indexL)
                            // e OBJECT_RANGE(bIndex) requer bIndex < MAX_OBJECTS.
                            // XOR 0xEFF5 é APENAS para ataques normais (opcode 0x11).
};
static std::vector<SkillPacketEntry> g_SkillQueue;
static CRITICAL_SECTION g_SkillQueueLock;

// ---- Overlapped WSARecv completion routine hook ----
// O jogo usa WSARecv com completion routine (I/O overlapped). O HookWSARecv
// só captura quando r==0 (completa imediato), mas overlapped retorna
// WSA_IO_PENDING e os dados chegam via completion routine. Embrulhamos a
// completion routine para logar os dados recebidos (RECV).
struct OverlappedRecvCtx {
    SOCKET s;
    LPWSABUF buffers;
    DWORD bufferCount;
    LPWSAOVERLAPPED_COMPLETION_ROUTINE original;
};
static std::map<LPWSAOVERLAPPED, OverlappedRecvCtx> g_OverlappedRecvMap;
static CRITICAL_SECTION g_OverlappedLock;
static bool g_OverlappedInit = false;

// Socket de controle: conexao com MuProxy2 em 127.0.0.1:44412
// Nao redirecionamos o IP/porta deste socket e nao logamos seus pacotes.
static bool IsControlSocket(SOCKET s)
{
    if (s == INVALID_SOCKET || g_TcpSocket == INVALID_SOCKET) return false;
    return s == g_TcpSocket;
}

static int (WINAPI* TrueSend)(SOCKET s, const char* buf, int len, int flags) = nullptr;
static int (WINAPI* TrueRecv)(SOCKET s, char* buf, int len, int flags) = nullptr;
static int (WINAPI* TrueWSASend)(SOCKET s, LPWSABUF buffers, DWORD bufferCount,
    LPDWORD numberOfBytesSent, DWORD flags, LPWSAOVERLAPPED lpOverlapped,
    LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine) = nullptr;
static int (WINAPI* TrueWSARecv)(SOCKET s, LPWSABUF buffers, DWORD bufferCount,
    LPDWORD numberOfBytesRecvd, LPDWORD flags, LPWSAOVERLAPPED lpOverlapped,
    LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine) = nullptr;
static int (WINAPI* TrueSendTo)(SOCKET s, const char* buf, int len, int flags,
    const sockaddr* to, int tolen) = nullptr;
static int (WINAPI* TrueRecvFrom)(SOCKET s, char* buf, int len, int flags,
    sockaddr* from, int* fromlen) = nullptr;
static int (WINAPI* TrueConnect)(SOCKET s, const sockaddr* name, int namelen) = nullptr;
static int (WINAPI* TrueWSAConnect)(SOCKET s, const sockaddr* name, int namelen,
    LPWSABUF callerData, LPWSABUF calleeData,
    LPQOS sqos, LPQOS gqos) = nullptr;

// Forward: completion routine wrapper (definida após HookWSARecv)
static void CALLBACK OverlappedRecvCompletion(DWORD dwError, DWORD cbTransferred,
    LPWSAOVERLAPPED lpOverlapped, DWORD dwFlags);

static void EnsureWinsockStarted()
{
    static bool started = false;
    if (!started)
    {
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) == 0) started = true;
    }
}

static void CloseTcpSocket()
{
    if (g_TcpSocket != INVALID_SOCKET)
    {
        closesocket(g_TcpSocket);
        g_TcpSocket = INVALID_SOCKET;
    }
}

static bool OpenTcpSocket()
{
    if (g_TcpSocket != INVALID_SOCKET) return true;
    EnsureWinsockStarted();

    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return false;

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((u_short)Config::PACKET_SNIFFER_TCP_PORT);
    inet_pton(AF_INET, Config::PACKET_SNIFFER_TCP_HOST, &addr.sin_addr);

    int result = TrueConnect ? TrueConnect(s, (sockaddr*)&addr, sizeof(addr))
                             : connect(s, (sockaddr*)&addr, sizeof(addr));
    if (result == SOCKET_ERROR)
    {
        int err = WSAGetLastError();
        closesocket(s);
        // Log com throttle (10s): TcpConnectThread tenta a cada 1s, sem isso
        // o log encheria durante o tempo que o MuProxy2 nao estiver ouvindo.
        static DWORD lastFailLog = 0;
        DWORD now = GetTickCount();
        if (now - lastFailLog > 10000)
        {
            lastFailLog = now;
            Logger::Log("PACKETSNIFFER", "Falha ao conectar %s:%d (err=%d)",
                        Config::PACKET_SNIFFER_TCP_HOST, Config::PACKET_SNIFFER_TCP_PORT, err);
        }
        return false;
    }

    u_long mode = 1;
    ioctlsocket(s, FIONBIO, &mode);
    g_TcpSocket = s;
    Logger::Log("PACKETSNIFFER", "Conectado ao MuProxy2 via TCP %s:%d",
                Config::PACKET_SNIFFER_TCP_HOST, Config::PACKET_SNIFFER_TCP_PORT);
    return true;
}

static DWORD WINAPI TcpConnectThread(LPVOID)
{
    while (g_Running)
    {
        EnterCriticalSection(&g_TcpLock);
        if (g_TcpSocket == INVALID_SOCKET) OpenTcpSocket();
        LeaveCriticalSection(&g_TcpLock);
        Sleep(1000);
    }
    return 0;
}

// Forward declarations
static void GameServerFullEncrypt(char* data, int len, BYTE xorKey, BYTE subKey);

// ============================================================================
// Receive thread: le comandos do MuProxy2 (SEND <hex>)
// ============================================================================
static DWORD WINAPI TcpRecvThread(LPVOID)
{
    char buf[8192];
    int bufPos = 0;

    while (g_Running)
    {
        EnterCriticalSection(&g_TcpLock);
        bool connected = (g_TcpSocket != INVALID_SOCKET);
        SOCKET s = g_TcpSocket;
        LeaveCriticalSection(&g_TcpLock);

        if (!connected) { Sleep(500); continue; }

        // Usa select() para esperar dados sem busy-wait
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(s, &readSet);
        timeval timeout = { 0, 500000 }; // 500ms
        int sel = select(0, &readSet, NULL, NULL, &timeout);
        if (sel <= 0) continue;

        // Le dados disponiveis
        char tmp[4096];
        int r = recv(s, tmp, sizeof(tmp) - 1, 0);
        if (r == SOCKET_ERROR)
        {
            int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK) continue;
            Logger::Log("PACKETSNIFFER", "TcpRecvThread: recv error %d", err);
            CloseTcpSocket();
            continue;
        }
        if (r == 0)
        {
            Logger::Log("PACKETSNIFFER", "TcpRecvThread: conexao fechada");
            CloseTcpSocket();
            continue;
        }
        tmp[r] = '\0';

        // Acumula no buffer e processa linhas completas
        for (int i = 0; i < r; i++)
        {
            if (tmp[i] == '\n')
            {
                buf[bufPos] = '\0';
                if (bufPos > 0)
                {
                    // Comando SEND [dir] <hex>  (dir opcional, ignorado - sempre injeta C->S)
                    if (strncmp(buf, "SEND ", 5) == 0)
                    {
                        char* args = buf + 5;
                        // Pula token opcional de direcao
                        char* hex = args;
                        if (strncmp(args, "Client ", 7) == 0 ||
                            strncmp(args, "Server ", 7) == 0 ||
                            strncmp(args, "C->S ", 5) == 0 ||
                            strncmp(args, "S->C ", 5) == 0)
                        {
                            // Avanca ate o proximo espaco
                            while (*hex && *hex != ' ') hex++;
                            while (*hex == ' ') hex++;
                        }

                        int hexLen = (int)strlen(hex);
                        BYTE packet[2048];
                        int packetLen = 0;
                        for (int j = 0; j < hexLen; j += 2)
                        {
                            if (hex[j] == ' ' || hex[j] == '\r' || hex[j] == '\0') { j--; continue; }
                            if (hex[j+1] == '\0') break;
                            unsigned int byteVal;
                            char tmpHex[3] = { hex[j], hex[j+1], 0 };
                            if (sscanf_s(tmpHex, "%02X", &byteVal) == 1)
                                packet[packetLen++] = (BYTE)byteVal;
                        }

                        if (packetLen > 0)
                        {
                            InjectToServer(packet, packetLen);
                        }
                    }
                }
                bufPos = 0;
            }
            else if (bufPos < (int)sizeof(buf) - 1)
            {
                buf[bufPos++] = tmp[i];
            }
        }
    }
    return 0;
}

static void SendLine(const char* line, int len)
{
    EnterCriticalSection(&g_TcpLock);
    if (!OpenTcpSocket())
    {
        LeaveCriticalSection(&g_TcpLock);
        return;
    }
    int sent = 0;
    while (sent < len)
    {
        int r = TrueSend ? TrueSend(g_TcpSocket, line + sent, len - sent, 0)
                         : send(g_TcpSocket, line + sent, len - sent, 0);
        if (r == SOCKET_ERROR)
        {
            int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK)
            {
                Sleep(1);
                continue;
            }
            CloseTcpSocket();
            LeaveCriticalSection(&g_TcpLock);
            return;
        }
        sent += r;
    }
    LeaveCriticalSection(&g_TcpLock);
}
static HMODULE GetMainDllModule()
{
    return (HMODULE)GameMemory::GetMainDllBase();
}

static bool GetGameServerKeys(unsigned char* xorKey, unsigned char* subKey)
{
    // Chaves hardcoded - confirmadas via x32dbg em Main.dll+0x403F75/76
    *xorKey = 0x36;
    *subKey = 0x07;

    // Loga apenas uma vez
    static bool logged = false;
    if (!logged) {
        logged = true;
        HMODULE hMain = GetMainDllModule();
        if (hMain) {
            uintptr_t base = (uintptr_t)hMain;
            unsigned char memSub = *(unsigned char*)(base + 0x403F75);
            unsigned char memXor = *(unsigned char*)(base + 0x403F76);
            Logger::Log("PACKETSNIFFER", "Chaves: xor=0x%02X sub=0x%02X (mem: xor=0x%02X sub=0x%02X)",
                *xorKey, *subKey, memXor, memSub);
        } else {
            Logger::Log("PACKETSNIFFER", "Chaves: xor=0x%02X sub=0x%02X", *xorKey, *subKey);
        }
    }
    return true;
}

// Porta do game server detectada automaticamente via pacote C116
// (0xC1 0x16 = "ConnectToServer": login server informa IP+porta do GS).
// 0 = ainda nao detectado (usa o range hardcoded abaixo).
static int g_DetectedGsPort = 0;

// Portas do game server (apenas trafego nelas sera decifrado)
static bool IsGameServerPort(int port) {
    if (port == 55902 || (port >= 55901 && port <= 55950)) return true;
    if (g_DetectedGsPort != 0 && port == g_DetectedGsPort) return true;
    return false;
}

// ============================================================================
// Detecção da porta do GS via C116 (ConnectToServer).
// O login server NAO cifra o trafego (em claro). Formato real (22 bytes):
//   C1 16 [charID 2B LE] [IP 16B null-padded] [porta 2B LE]
// Exemplo capturado: C1 16 F4 03 "145.239.149.84\0.." 4D B3  -> porta 45901.
// Escaneamos o buffer RECV pela assinatura em claro C1 16, validamos que o
// campo IP comeca com digito e lemos a porta no offset 20 (little-endian).
// ============================================================================
static void TryDetectGsPortFromC116(const char* data, int len)
{
    const int C116_LEN = 22;
    if (len < C116_LEN) return;
    for (int i = 0; i + C116_LEN <= len; ++i)
    {
        // Assinatura em claro de C1 16 (login server nao cifra)
        if ((unsigned char)data[i] != 0xC1 || (unsigned char)data[i + 1] != 0x16) continue;

        // Campo IP (offset 4) deve comecar com digito (primeiro char de um IPv4)
        if (data[i + 4] < '0' || data[i + 4] > '9') continue;

        // Porta no offset 20 (2B header + 2B charID + 16B IP buffer), little-endian
        int port = ((unsigned char)data[i + 21] << 8) | (unsigned char)data[i + 20];
        if (port < 1024 || port > 65535) continue;

        if (g_DetectedGsPort != port)
        {
            g_DetectedGsPort = port;
            Logger::Log("PACKETSNIFFER", "C116: porta do GS detectada = %d", port);
        }
        return;
    }
}

// Armazena a porta remota por socket para filtrar trafego
static bool GetSocketPort(SOCKET s, int* outPort) {
    sockaddr_in addr;
    int addrLen = sizeof(addr);
    if (getpeername(s, (sockaddr*)&addr, &addrLen) == 0) {
        *outPort = ntohs(addr.sin_port);
        return true;
    }
    return false;
}

static void GameServerFullEncrypt(char* data, int len, BYTE xorKey, BYTE subKey);

// ============================================================================
// XorFilter — Camada 2 de criptografia (PacketManager::XorData)
// Tabela REAL lida do binario Main.dll em 0x7166FF20 (via x32dbg)
// ============================================================================
static const BYTE g_XorFilter[32] = {
    0xAB, 0x11, 0xCD, 0xFE, 0x18, 0x23, 0xC5, 0xA3,
    0xCA, 0x33, 0xC1, 0xCC, 0x66, 0x67, 0x21, 0xF3,
    0x32, 0x12, 0x15, 0x35, 0x29, 0xFF, 0xFE, 0x1D,
    0x44, 0xEF, 0xCD, 0x41, 0x26, 0x3C, 0x4E, 0x4D
};

static int GetMuHeaderSize(BYTE firstByte)
{
    switch (firstByte) {
        case 0xC1: case 0xC3: return 2;
        case 0xC2: case 0xC4: return 3;
        default: return 0;
    }
}

// ============================================================================
// Camada 1: XOR+sub byte-a-byte (HackCheck::DecryptData)
// ============================================================================
static void GameServerDecryptLayer1(char* data, int len, BYTE xorKey, BYTE subKey)
{
    for (int i = 0; i < len; ++i)
    {
        unsigned char c = (unsigned char)data[i];
        data[i] = (char)((c ^ xorKey) - subKey);
    }
}

// ============================================================================
// Camada 1: XOR+sub byte-a-byte (HackCheck::EncryptData)
// ============================================================================
static void GameServerEncryptLayer1(char* data, int len, BYTE xorKey, BYTE subKey)
{
    for (int i = 0; i < len; ++i)
    {
        unsigned char c = (unsigned char)data[i];
        data[i] = (char)(((c + subKey) ^ xorKey));
    }
}

// ============================================================================
// Camada 2: XOR encadeado no corpo do pacote (PacketManager::XorData)
// ============================================================================
static void GameServerXorData(char* data, int start, int end)
{
    if (start >= end) return;
    for (int n = start; n < end; ++n)
    {
        data[n] ^= (data[n - 1] ^ g_XorFilter[n % 32]);
    }
}

// ============================================================================
// Decifracao: Camada 1 (XOR+sub) — usada para display no log.
//
// ATENCAO (RE validada, 4 capturas): o BlackRock USa XorFilter (Camada 2) nos
// pacotes de ataque C1 0x11. A cadeia real e:
//   WIRE = Layer1( XorData[3,7)( plaintext ) )
// Decifrar SO com Layer-1 (esta funcao) devolve a forma XorData
// (ex.: "C1 07 11 EF ED B6 77"), NAO o plaintext real ("C1 07 11 00 1A 78 04").
// Para o plaintext real, decifre Layer-1 e depois aplique GameServerXorData
// (que e sua propria inversa) em [3, len).
// O uso de XorData em outros opcodes C1 (ex.: movimento D4) ainda nao confirmado.
// ============================================================================
static void GameServerFullDecrypt(char* data, int len, BYTE xorKey, BYTE subKey)
{
    GameServerDecryptLayer1(data, len, xorKey, subKey);
}

// ============================================================================
// Cifracao: APENAS Camada 1 (XOR+sub)
// ============================================================================
static void GameServerFullEncrypt(char* data, int len, BYTE xorKey, BYTE subKey)
{
    GameServerEncryptLayer1(data, len, xorKey, subKey);
}

static CRITICAL_SECTION g_ReasmLock;
static bool g_ReasmInit = false;
static std::map<SOCKET, std::vector<BYTE>> g_ReasmBuffers;

static std::vector<BYTE>* GetReasmBuffer(SOCKET s)
{
    if (!g_ReasmInit) {
        InitializeCriticalSection(&g_ReasmLock);
        g_ReasmInit = true;
    }
    EnterCriticalSection(&g_ReasmLock);
    std::vector<BYTE>* buf = &g_ReasmBuffers[s];
    LeaveCriticalSection(&g_ReasmLock);
    return buf;
}

static void RemoveReasmBuffer(SOCKET s)
{
    if (!g_ReasmInit) return;
    EnterCriticalSection(&g_ReasmLock);
    g_ReasmBuffers.erase(s);
    LeaveCriticalSection(&g_ReasmLock);
}

static int ReadMuPacketLength(const std::vector<BYTE>& buf)
{
    if (buf.empty()) return 0;
    BYTE h = buf[0];
    int hdr = GetMuHeaderSize(h);
    if ((int)buf.size() < hdr) return 0;
    if (h == 0xC1 || h == 0xC3) return buf[1];
    if (h == 0xC2 || h == 0xC4) return (buf[1] << 8) | buf[2];
    return 0;
}

static void FlushReasmBuffer(SOCKET s, const char* dir, unsigned char xorKey, unsigned char subKey, bool isGameServer)
{
    std::vector<BYTE>* buf = GetReasmBuffer(s);
    int emitted = 0;
    while (!buf->empty())
    {
        int pktLen = ReadMuPacketLength(*buf);
        if (pktLen == 0 || (int)buf->size() < pktLen) break;
        // Sanity: pacote nao pode ser maior que buffer acumulado excessivo
        if (pktLen > 4096) {
            buf->clear();
            RemoveReasmBuffer(s);
            break;
        }

        std::vector<BYTE> pkt(buf->begin(), buf->begin() + pktLen);
        buf->erase(buf->begin(), buf->begin() + pktLen);

        char outBuf[4096];
        int copyLen = (int)pkt.size() < (int)sizeof(outBuf) ? (int)pkt.size() : (int)sizeof(outBuf);
        memcpy(outBuf, pkt.data(), copyLen);

        if (isGameServer)
            GameServerFullDecrypt(outBuf, copyLen, xorKey, subKey);

        // Parse de pacotes RECV (já decifrados) para alimentar os mapas dinâmicos.
        // Só processa tráfego do game server (porta 55902).
        if (isGameServer && dir && dir[0] == 'R') {
            int hdr = GetMuHeaderSize((BYTE)outBuf[0]);
            if (hdr > 0 && copyLen > hdr) {
                BYTE opcode = (BYTE)outBuf[hdr];
                if (opcode == 0x13) {
                    ViewportMap::OnViewportPacket((const uint8_t*)outBuf, copyLen);
                } else if (opcode == 0xF3 && copyLen > hdr + 1) {
                    BYTE sub = (BYTE)outBuf[hdr + 1];
                    if (sub == 0x11 || sub == 0xE2) {
                        SkillMap::OnSkillListPacket((const uint8_t*)outBuf, copyLen);
                    }
                }
            }
        }

        // Envia ao proxy (UM pacote por linha PKT|dir|hex)
        char line[8192];
        int pos = snprintf(line, sizeof(line), "PKT|%s|", dir);
        if (pos <= 0 || pos >= (int)sizeof(line)) continue;
        for (int i = 0; i < copyLen && pos + 2 < (int)sizeof(line); ++i)
            pos += snprintf(line + pos, sizeof(line) - pos, "%02X", (unsigned char)outBuf[i]);
        if (pos + 1 < (int)sizeof(line))
            line[pos++] = '\n';
        else
            continue;
        SendLine(line, pos);
        emitted++;
    }
    // Evita vazamento infinito se bytes lixo/nao-MU acumularem
    if (buf->size() > 32768) {
        buf->clear();
    }
}

// ============================================================================
// LogRaw: reassemble + envia bytes DECIFRADOS ao proxy. Nao loga hex bruto
// (o log detalhado fica no HookSend/HookWSASend). So decifra trafego do
// game server (porta 55902).
// ============================================================================
static void LogRaw(const char* dir, SOCKET s, const char* data, int len)
{
    if (len <= 0 || !data) return;

    // Pula o socket de controle do MuProxy
    if (IsControlSocket(s)) return;

    // Pula mensagens de LOG (comecam com "LOG " = 0x4C 0x4F 0x47 0x20)
    if (len >= 4 && (unsigned char)data[0] == 0x4C && (unsigned char)data[1] == 0x4F
        && (unsigned char)data[2] == 0x47 && (unsigned char)data[3] == 0x20)
        return;

    // Detecta a porta do GS via C116 (ConnectToServer) no trafego RECV
    // (login server -> cliente). O pacote chega cifrado, entao escaneamos os bytes crus.
    if (dir && dir[0] == 'R')
        TryDetectGsPortFromC116(data, len);

    int port = 0;
    bool isGameServer = GetSocketPort(s, &port) && IsGameServerPort(port);

    unsigned char xorKey = 0, subKey = 0;
    if (isGameServer) GetGameServerKeys(&xorKey, &subKey);

    // Decifra pacotes do game server para logar em claro
    char decrypted[4096];
    const char* src = data;
    int srcLen = len;
    if (isGameServer) {
        int copyLen = len < (int)sizeof(decrypted) ? len : (int)sizeof(decrypted);
        memcpy(decrypted, data, copyLen);
        GameServerFullDecrypt(decrypted, copyLen, xorKey, subKey);
        src = decrypted;
        srcLen = copyLen;
    }

    // Nao loga hex bruto aqui (poluicao). O log detalhado (stack trace, C3/C4
    // decrypt, SMOD) e feito no HookSend/HookWSASend. So reassemble + proxy.

    std::vector<BYTE>* buf = GetReasmBuffer(s);
    buf->insert(buf->end(), (const BYTE*)src, (const BYTE*)src + srcLen);
    FlushReasmBuffer(s, dir, xorKey, subKey, false); // buffer ja esta decifrado/claro
}

// ============================================================================
// Character Creation Rewrite: substitui nome/classe no pacote 0x31
// Algoritmo de encoding (descoberto via engenharia reversa):
//   key = [CF, DB, 23, C5, A3, CA, 33, C1, CC, 66]
//   enc[0] = name[0] ^ key[0]
//   enc[1] = name[1] ^ key[1]
//   enc[i] = name[i] ^ enc[i-1] ^ key[i]  (i >= 2)
//   classByte = class ^ enc[8] ^ 0x63
// ============================================================================
static const BYTE g_NameKey[10] = { 0xCF, 0xDB, 0x23, 0xC5, 0xA3, 0xCA, 0x33, 0xC1, 0xCC, 0x66 };
static const BYTE g_ClassKey = 0x63;

// Nome e classe desejados
static const char* g_RewriteName = "HexDL";
static const BYTE g_RewriteClass = 0x10; // Dark Lord (client-side)

static void EncodeName(const char* name, BYTE classId, BYTE* outEncoded)
{
    int nameLen = (int)strlen(name);
    for (int i = 0; i < 10; i++) {
        BYTE p = (i < nameLen) ? (BYTE)name[i] : 0;
        if (i == 0)
            outEncoded[i] = p ^ g_NameKey[0];
        else if (i == 1)
            outEncoded[i] = p ^ g_NameKey[1];
        else
            outEncoded[i] = p ^ outEncoded[i-1] ^ g_NameKey[i];
    }
    outEncoded[10] = classId ^ outEncoded[8] ^ g_ClassKey;
}

static bool IsCreateCharPacket(const char* data, int len)
{
    if (len < 15) return false;
    return (BYTE)data[0] == 0xC1 && (BYTE)data[1] == 0x0F
        && (BYTE)data[2] == 0x31 && (BYTE)data[3] == 0xC3;
}

// Forward: processa fila de skill attacks (definida abaixo)
static void ProcessSkillQueue();

static int WINAPI HookSend(SOCKET s, const char* buf, int len, int flags)
{
    if (buf && len > 0) {
        // Pula texto ASCII (comandos INJECT etc)
        if (buf[0] >= 0x20 && buf[0] < 0x7F) {
            return TrueSend(s, buf, len, flags);
        }

        // Fallback: se o socket do GS ainda nao foi capturado pelo connect,
        // captura aqui no primeiro send do jogo (ex: pacote de login)
        if (g_GameServerSocket == INVALID_SOCKET) {
            int port = 0;
            if (GetSocketPort(s, &port) && IsGameServerPort(port)) {
                g_GameServerSocket = s;
                Logger::Log("PACKETSNIFFER", "Game server socket rastreado via SEND: %d (porta %d)", (int)s, port);
            }
        }

        // Pega EBP para stack trace completo
        uintptr_t* ebpFrame;
        __asm { mov ebpFrame, ebp }

        // ---- Decifra Layer-1 (XOR 0x36, sub 0x07) para mostrar plaintext no log ----
        // O jogo envia bytes cifrados (headers FC/FE), mas queremos ver C1/C2/C3/C4.
        // Aplica GameServerFullDecrypt (apenas Layer-1, sem XorFilter) em uma copia.
        int port = 0;
        bool isGS = GetSocketPort(s, &port) && IsGameServerPort(port);
        char decBuf[4096];
        const char* displayBuf = buf;
        int displayLen = len;
        if (isGS) {
            int copyLen = len < (int)sizeof(decBuf) ? len : (int)sizeof(decBuf);
            memcpy(decBuf, buf, copyLen);
            unsigned char xorKey = 0, subKey = 0;
            GetGameServerKeys(&xorKey, &subKey);
            GameServerFullDecrypt(decBuf, copyLen, xorKey, subKey);
            displayBuf = decBuf;
            displayLen = copyLen;
        }

        // Hex do pacote (decifrado se for game server)
        char hex[256];
        int hpos = 0;
        int blen = displayLen > 48 ? 48 : displayLen;
        for (int j = 0; j < blen; ++j)
            hpos += snprintf(hex + hpos, sizeof(hex) - hpos, "%02X ", (unsigned char)displayBuf[j]);

        // Header type legivel: C1/C2/C3/C4 ou "??"
        const char* hdrType = "??";
        if (displayLen > 0) {
            switch ((unsigned char)displayBuf[0]) {
                case 0xC1: hdrType = "C1"; break;
                case 0xC2: hdrType = "C2"; break;
                case 0xC3: hdrType = "C3"; break;
                case 0xC4: hdrType = "C4"; break;
            }
        }

        Logger::Log("PACKETSNIFFER", "SEND %s %d bytes [%s]",
                    hdrType, displayLen, hex);
        LogStackTrace("SEND stack", ebpFrame);

        // Decifra C3/C4 com a nossa Decrypt (DecKey = inversa modular do EncKey).
        // O NativeDecrypt do jogo falha em C2S: o DecKey do Obj1 (0x8797778) e zero.
        if (isGS && displayLen > 2 && ((unsigned char)displayBuf[0] == 0xC3 || (unsigned char)displayBuf[0] == 0xC4)) {
            uint8_t pktHdr = (unsigned char)displayBuf[0];
            int hdrSize = (pktHdr == 0xC3) ? 2 : 3;  // C3=2 bytes header, C4=3 bytes header
            int smLen = displayLen - hdrSize;
            if (smLen >= SM_ENCRYPTED_SIZE) {
                // Log the exact bytes being passed to SM decrypt for debugging
                {
                    char smHex[128];
                    int sp = 0;
                    int sblen = smLen > 22 ? 22 : smLen;
                    for (int j = 0; j < sblen && sp < (int)sizeof(smHex) - 4; ++j)
                        sp += snprintf(smHex + sp, sizeof(smHex) - sp, "%02X ", (unsigned char)displayBuf[hdrSize+j]);
                    Logger::Log("SMOD", "SM input bytes (%d): [%s]", smLen, smHex);
                }
                
                uint8_t decrypted[256];
                bool decryptedOk = false;
                
                int decLen = SimpleModulusCipher::Decrypt(decrypted, displayBuf + hdrSize, smLen);
                if (decLen > 0) {
                    decryptedOk = true;
                    Logger::Log("PACKETSNIFFER", "SEND C3 decrypt: OK decLen=%d", decLen);
                } else {
                    Logger::Log("PACKETSNIFFER", "SEND C3 decrypt: FAILED (decLen=%d)", decLen);
                }
                
                if (decryptedOk) {
                    // C3/C4 SM decrypt format:
                    //   stream[0] = decKey (encryption key byte, not part of payload)
                    //   stream[1..validTotal-1] = C1/C2 payload (plaintext)
                    //
                    // BlackRock NÃO usa XOR C1C2 (tabela 0xE7,0x6D...):
                    //   - Evidência: C1 direto "C1 08 D4 02" (movimento) aparece com
                    //     opcode D4 após Layer-1 decrypt. Se XOR C1C2 fosse usado,
                    //     D4 seria codificado para 8A (calculado via Python).
                    //   - Evidência: C3 decrypt sem XOR dá opcode 0x19 (attack) correto.
                    //   - Evidência: MuProxy define EnableXorDecode=false por padrão.
                    //   - MuEncDec.DecodeC3C4 chama DecodeC1C2InPlace, mas isso é
                    //     desabilitado via DisableC1C2Xor=true no BlackRock.
                    //   - XorFilter (tabela 0xAB,0x11...) = Camada 2: NÃO usado no
                    //     caminho C3C4 (que usa SimpleModulus), MAS usado nos pacotes
                    //     de ataque C1 0x11 (ver InjectToServer / RE validada).
                    //
                    // Reconstruct: C3→C1, C4→C2
                    //   innerPkt = [C1/C2] [size] [stream[1]] ... [stream[validTotal-1]]
                    
                    uint8_t hdr = (uint8_t)displayBuf[0];
                    uint8_t innerHdr = (hdr == 0xC4) ? 0xC2 : 0xC1;
                    int innerHdrSize = (innerHdr == 0xC2) ? 3 : 2;
                    int contentBytes = decLen - 1;  // skip decKey byte
                    int innerTotalSize = innerHdrSize + contentBytes;
                    
                    if (contentBytes > 0) {
                        uint8_t innerPkt[256];
                        int pos = 0;
                        innerPkt[pos++] = innerHdr;
                        if (innerHdrSize == 2) {
                            innerPkt[pos++] = (uint8_t)innerTotalSize;
                        } else {
                            innerPkt[pos++] = (uint8_t)((innerTotalSize >> 8) & 0xFF);
                            innerPkt[pos++] = (uint8_t)(innerTotalSize & 0xFF);
                        }
                        // Copy stream[1..] as payload (skip decKey at stream[0])
                        memcpy(innerPkt + pos, decrypted + 1, contentBytes);
                        
                        // Log the decoded C1/C2 packet
                        char decHex[256];
                        int dp = 0;
                        for (int j = 0; j < innerTotalSize && dp < (int)sizeof(decHex) - 4; ++j)
                            dp += snprintf(decHex + dp, sizeof(decHex) - dp, "%02X ", innerPkt[j]);
                        Logger::Log("PACKETSNIFFER", "SEND %s decoded: [%s] dk=%02X",
                            (innerHdr == 0xC2) ? "C4>C2" : "C3>C1", decHex, decrypted[0]);
                    } else {
                        Logger::Log("PACKETSNIFFER", "SEND C3 decrypt: empty payload (decLen=%d)", decLen);
                    }
                }
            }
        }

        LogRaw("SEND", s, buf, len);
    }

    // Processa fila de skill attacks na thread do jogo (seguro chamar 0x998420)
    if (s == g_GameServerSocket) {
        ProcessSkillQueue();
    }

    return TrueSend(s, buf, len, flags);
}

static int WINAPI HookRecv(SOCKET s, char* buf, int len, int flags)
{
    int r = TrueRecv(s, buf, len, flags);
    if (r > 0) {
        Logger::Log("PACKETSNIFFER", "recv() ret=%d socket=%d", r, (int)s);
        LogRaw("RECV", s, buf, r);
    }
    if (r == 0) RemoveReasmBuffer(s); // conexao fechada
    return r;
}

static int WINAPI HookWSASend(SOCKET s, LPWSABUF buffers, DWORD bufferCount,
    LPDWORD numberOfBytesSent, DWORD flags, LPWSAOVERLAPPED lpOverlapped,
    LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
    // Loga TODO pacote enviado com hex dump + stack trace completo
    if (buffers)
    {
        for (DWORD i = 0; i < bufferCount; ++i)
        {
            if (buffers[i].buf && buffers[i].len > 0)
            {
                // Hex dump completo
                char hex[1024];
                int pos = 0;
                int blen = buffers[i].len;
                if (blen > 256) blen = 256;
                for (int j = 0; j < blen && pos < (int)sizeof(hex) - 4; ++j)
                    pos += snprintf(hex + pos, sizeof(hex) - pos, "%02X ", (unsigned char)buffers[i].buf[j]);
                Logger::Log("PACKETSNIFFER", "WSASend: %d bytes [%s]", buffers[i].len, hex);
                uintptr_t* callerEbp;
                __asm { mov callerEbp, ebp }
                LogStackTrace("WSASend stack", callerEbp);
                LogRaw("SEND", s, buffers[i].buf, buffers[i].len);
            }
        }
    }
    return TrueWSASend(s, buffers, bufferCount, numberOfBytesSent, flags, lpOverlapped, lpCompletionRoutine);
}

static int WINAPI HookWSARecv(SOCKET s, LPWSABUF buffers, DWORD bufferCount,
    LPDWORD numberOfBytesRecvd, LPDWORD flags, LPWSAOVERLAPPED lpOverlapped,
    LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
    // Embrulha a completion routine para capturar dados recebidos via overlapped I/O.
    // O jogo usa WSARecv overlapped: retorna WSA_IO_PENDING e os dados chegam
    // depois via completion routine. Sem este wrap, nenhum RECV é capturado.
    LPWSAOVERLAPPED_COMPLETION_ROUTINE wrapped = lpCompletionRoutine;
    if (lpCompletionRoutine && lpOverlapped) {
        if (!g_OverlappedInit) {
            InitializeCriticalSection(&g_OverlappedLock);
            g_OverlappedInit = true;
        }
        EnterCriticalSection(&g_OverlappedLock);
        OverlappedRecvCtx ctx;
        ctx.s = s;
        ctx.buffers = buffers;
        ctx.bufferCount = bufferCount;
        ctx.original = lpCompletionRoutine;
        g_OverlappedRecvMap[lpOverlapped] = ctx;
        LeaveCriticalSection(&g_OverlappedLock);
        wrapped = OverlappedRecvCompletion;
    }

    int r = TrueWSARecv(s, buffers, bufferCount, numberOfBytesRecvd, flags, lpOverlapped, wrapped);

    // Diagnóstico: qual mecanismo de recv o jogo usa?
    Logger::Log("PACKETSNIFFER", "WSARecv: ret=%d err=%d socket=%d overlapped=%p routine=%p",
                r, (r == SOCKET_ERROR) ? WSAGetLastError() : 0, (int)s, (void*)lpOverlapped, (void*)lpCompletionRoutine);

    // Completa imediato (r==0): loga direto e remove do map.
    if (r == 0 && buffers && numberOfBytesRecvd) {
        for (DWORD i = 0; i < bufferCount; ++i) {
            if (buffers[i].buf && numberOfBytesRecvd[i] > 0)
                LogRaw("RECV", s, buffers[i].buf, numberOfBytesRecvd[i]);
        }
        if (lpOverlapped) {
            EnterCriticalSection(&g_OverlappedLock);
            g_OverlappedRecvMap.erase(lpOverlapped);
            LeaveCriticalSection(&g_OverlappedLock);
        }
    }
    if (r != 0 && WSAGetLastError() != WSA_IO_PENDING)
        RemoveReasmBuffer(s);
    return r;
}

// Completion routine wrapper: loga dados recebidos e chama a original.
static void CALLBACK OverlappedRecvCompletion(DWORD dwError, DWORD cbTransferred,
    LPWSAOVERLAPPED lpOverlapped, DWORD dwFlags)
{
    OverlappedRecvCtx ctx;
    bool found = false;
    if (lpOverlapped) {
        EnterCriticalSection(&g_OverlappedLock);
        auto it = g_OverlappedRecvMap.find(lpOverlapped);
        if (it != g_OverlappedRecvMap.end()) {
            ctx = it->second;
            g_OverlappedRecvMap.erase(it);
            found = true;
        }
        LeaveCriticalSection(&g_OverlappedLock);
    }

    if (found && dwError == 0 && cbTransferred > 0 && ctx.buffers) {
        DWORD remaining = cbTransferred;
        for (DWORD i = 0; i < ctx.bufferCount && remaining > 0; ++i) {
            if (ctx.buffers[i].buf && ctx.buffers[i].len > 0) {
                DWORD n = ctx.buffers[i].len < remaining ? ctx.buffers[i].len : remaining;
                LogRaw("RECV", ctx.s, ctx.buffers[i].buf, (int)n);
                remaining -= n;
            }
        }
    }

    if (found && ctx.original)
        ctx.original(dwError, cbTransferred, lpOverlapped, dwFlags);
}

static int WINAPI HookSendTo(SOCKET s, const char* buf, int len, int flags,
    const sockaddr* to, int tolen)
{
    if (buf && len > 0) LogRaw("SEND", s, buf, len);
    return TrueSendTo(s, buf, len, flags, to, tolen);
}

static int WINAPI HookRecvFrom(SOCKET s, char* buf, int len, int flags,
    sockaddr* from, int* fromlen)
{
    int r = TrueRecvFrom(s, buf, len, flags, from, fromlen);
    if (r > 0) LogRaw("RECV", s, buf, r);
    return r;
}

static int WINAPI HookConnect(SOCKET s, const sockaddr* name, int namelen)
{
    if (name && namelen >= sizeof(sockaddr_in))
    {
        sockaddr_in target = *(sockaddr_in*)name;
        char ip[16];
        strncpy_s(ip, inet_ntoa(target.sin_addr), 15);
        int port = ntohs(target.sin_port);
        Logger::Log("PACKETSNIFFER", "CONNECT to %s:%d", ip, port);

        // Rastreia socket do game server para injects
        if (IsGameServerPort(port))
        {
            g_GameServerSocket = s;
            Logger::Log("PACKETSNIFFER", "Game server socket rastreado: %d", (int)s);
        }

        target.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
        return TrueConnect(s, (sockaddr*)&target, sizeof(target));
    }
    return TrueConnect(s, name, namelen);
}

static int WINAPI HookWSAConnect(SOCKET s, const sockaddr* name, int namelen,
    LPWSABUF callerData, LPWSABUF calleeData, LPQOS sqos, LPQOS gqos)
{
    if (name && namelen >= sizeof(sockaddr_in))
    {
        sockaddr_in target = *(sockaddr_in*)name;
        char ip[16];
        strncpy_s(ip, inet_ntoa(target.sin_addr), 15);
        int port = ntohs(target.sin_port);
        Logger::Log("PACKETSNIFFER", "WSACONNECT to %s:%d", ip, port);

        // Rastreia socket do game server para injects
        if (IsGameServerPort(port))
        {
            g_GameServerSocket = s;
            Logger::Log("PACKETSNIFFER", "Game server socket rastreado: %d", (int)s);
        }

        target.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
        return TrueWSAConnect(s, (sockaddr*)&target, sizeof(target), callerData, calleeData, sqos, gqos);
    }
    return TrueWSAConnect(s, name, namelen, callerData, calleeData, sqos, gqos);
}

#pragma pack(push, 1)
struct Trampoline {
    BYTE origBytes[5];
    BYTE jmp;
    DWORD rel;
};

struct InlineHookState {
    void* target;
    BYTE original[5];
    Trampoline* trampoline;
    bool installed;
};
#pragma pack(pop)

static InlineHookState g_Hooks[16];
static int g_HookCount = 0;

static Trampoline* AllocateTrampoline()
{
    return (Trampoline*)VirtualAlloc(nullptr, sizeof(Trampoline), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
}

static bool InstallInlineHook(void* target, void* hook, InlineHookState* state)
{
    if (!target || !hook || !state) return false;
    if (state->installed) return true;

    DWORD oldProtect = 0;
    if (!VirtualProtect(target, 5, PAGE_EXECUTE_READWRITE, &oldProtect))
    {
        Logger::Log("PACKETSNIFFER", "VirtualProtect falhou no target %p", target);
        return false;
    }

    BYTE* t = (BYTE*)target;
    memcpy(state->original, t, 5);

    Trampoline* tramp = AllocateTrampoline();
    if (!tramp)
    {
        VirtualProtect(target, 5, oldProtect, &oldProtect);
        Logger::Log("PACKETSNIFFER", "Falha ao alocar trampoline");
        return false;
    }

    memcpy(tramp->origBytes, state->original, 5);
    tramp->jmp = 0xE9;
    tramp->rel = (DWORD)((t + 5) - ((BYTE*)tramp + 10));

    state->target = target;
    state->trampoline = tramp;
    state->installed = true;

    LONG rel = (LONG)((BYTE*)hook - (t + 5));
    t[0] = 0xE9;
    memcpy(t + 1, &rel, 4);

    VirtualProtect(target, 5, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), target, 5);
    FlushInstructionCache(GetCurrentProcess(), tramp, sizeof(Trampoline));
    return true;
}

static void* GetExportAddress(const char* name)
{
    HMODULE hWs2 = GetModuleHandleA("ws2_32.dll");
    if (!hWs2) return nullptr;
    return GetProcAddress(hWs2, name);
}

bool Install()
{
    if (g_Initialized) return true;
    InitializeCriticalSection(&g_TcpLock);
    InitializeCriticalSection(&g_SkillQueueLock);
    g_Initialized = true;
    g_Running = true;
    g_ConnectThread = CreateThread(NULL, 0, TcpConnectThread, NULL, 0, NULL);
    g_RecvThread = CreateThread(NULL, 0, TcpRecvThread, NULL, 0, NULL);

    Logger::Log("PACKETSNIFFER", "Instalando inline hooks nas exports do ws2_32.dll");

    struct { const char* name; void* hook; void** original; } targets[] = {
        { "send", (void*)HookSend, (void**)&TrueSend },
        { "recv", (void*)HookRecv, (void**)&TrueRecv },
        { "WSASend", (void*)HookWSASend, (void**)&TrueWSASend },
        { "WSARecv", (void*)HookWSARecv, (void**)&TrueWSARecv },
        { "sendto", (void*)HookSendTo, (void**)&TrueSendTo },
        { "recvfrom", (void*)HookRecvFrom, (void**)&TrueRecvFrom },
        { "connect", (void*)HookConnect, (void**)&TrueConnect },
        { "WSAConnect", (void*)HookWSAConnect, (void**)&TrueWSAConnect },
    };

    for (int i = 0; i < 8; ++i)
    {
        void* target = GetExportAddress(targets[i].name);
        if (!target)
        {
            Logger::Log("PACKETSNIFFER", "%s nao encontrado no ws2_32", targets[i].name);
            continue;
        }

        if (InstallInlineHook(target, targets[i].hook, &g_Hooks[g_HookCount]))
        {
            *targets[i].original = g_Hooks[g_HookCount].trampoline;
            Logger::Log("PACKETSNIFFER", "Hook instalado em ws2_32.%s -> %p (trampoline=%p)",
                targets[i].name, target, *targets[i].original);
            g_HookCount++;
        }
        else
        {
            Logger::Log("PACKETSNIFFER", "FALHA ao hookar ws2_32.%s", targets[i].name);
        }
    }

    Logger::Log("PACKETSNIFFER", "Total de hooks instalados=%d", g_HookCount);
    return g_HookCount > 0;
}

void InstallHooksForAllModules()
{
    Install();
}

void Uninstall()
{
    g_Running = false;
    if (g_ConnectThread)
    {
        WaitForSingleObject(g_ConnectThread, 1000);
        CloseHandle(g_ConnectThread);
        g_ConnectThread = NULL;
    }

    for (int i = 0; i < g_HookCount; ++i)
    {
        if (!g_Hooks[i].installed || !g_Hooks[i].target) continue;
        DWORD oldProtect = 0;
        if (VirtualProtect(g_Hooks[i].target, 5, PAGE_EXECUTE_READWRITE, &oldProtect))
        {
            memcpy(g_Hooks[i].target, g_Hooks[i].original, 5);
            VirtualProtect(g_Hooks[i].target, 5, oldProtect, &oldProtect);
            FlushInstructionCache(GetCurrentProcess(), g_Hooks[i].target, 5);
        }
        if (g_Hooks[i].trampoline)
        {
            VirtualFree(g_Hooks[i].trampoline, 0, MEM_RELEASE);
        }
        g_Hooks[i].installed = false;
    }
    g_HookCount = 0;

    CloseTcpSocket();
    g_GameServerSocket = INVALID_SOCKET;
    if (g_RecvThread)
    {
        WaitForSingleObject(g_RecvThread, 1000);
        CloseHandle(g_RecvThread);
        g_RecvThread = NULL;
    }
    if (g_Initialized)
    {
        DeleteCriticalSection(&g_TcpLock);
        DeleteCriticalSection(&g_SkillQueueLock);
        g_Initialized = false;
    }
}

// ============================================================================
// EnqueueSkillPacket — Thread-safe: AttackModule chama de qualquer thread.
// HookSend processa na thread do jogo chamando SimpleModulus::Encrypt (0x998420).
// ============================================================================
bool EnqueueSkillPacket(const uint8_t* plaintext, int len, SkillPacketType type,
                       uint16_t skillId, uint16_t attackId) {
    if (!plaintext || len <= 0 || len > 8) return false;

    SkillPacketEntry entry;
    memcpy(entry.data, plaintext, len);
    entry.len = len;
    entry.type = type;
    entry.skillId = skillId;
    entry.attackId = attackId;

    EnterCriticalSection(&g_SkillQueueLock);
    g_SkillQueue.push_back(entry);
    LeaveCriticalSection(&g_SkillQueueLock);
    return true;
}

// ============================================================================
// ProcessSkillQueue — Chamado pelo HookSend na thread do jogo.
//
// ABORDAGEM NOVA: Usa as funções BUILDER nativas do jogo para montar o
// pacote de skill attack, em vez de criar o struct manualmente.
//
// O jogo monta o pacote assim (trace x64dbg @ 0x59CE53):
//   1. lea ecx, [ebp-0x81C]           ; builder object (0x81C bytes!)
//   2. call 0x404B40                   ; init builder
//   3. push 0x19; push 0xC1
//   4. lea ecx, [ebp-0x81C]
//   5. call 0x404B90                   ; setHeader(this, 0xC1, 0x19=25)
//   6. mov edx, [0x081C03A4]           ; target entity index (GLOBAL)
//   7. and edx, 0xFF                   ; target low byte
//   8. push edx                        ; arg4 (targetL)
//   9. mov eax, [0x081C03A4]           ; target entity index again
//  10. sar eax, 8                     ; target high byte
//  11. push eax                        ; arg3 (targetH)
//  12. movzx ecx, [ebp-0x820]         ; header/skillId word
//  13. and ecx, 0xFF                   ; skill low byte
//  14. push ecx                        ; arg2 (skillL)
//  15. movzx edx, [ebp-0x820]         ; header/skillId word again
//  16. shr edx, 8                     ; skill high byte
//  17. and edx, 0xFF
//  18. push edx                        ; arg1 (skillH)
//  19. lea ecx, [ebp-0x81C]
//  20. call 0x4050D0                   ; addData — consome arg1 (skillH)
//  21. mov ecx, eax
//  22. call 0x4050D0                   ; addData — consome arg2 (skillL)
//  23. mov ecx, eax
//  24. call 0x4050D0                   ; addData — consome arg3 (targetH)
//  25. mov ecx, eax
//  26. call 0x4050D0                   ; addData — consome arg4 (targetL)
//  27. push 0; push 1
//  28. lea ecx, [ebp-0x81C]
//  29. call 0x404D70                   ; encrypt & send(this, 1, 0)
//  30. lea ecx, [ebp-0x81C]
//  31. call 0x404B60                   ; destroy builder
//
// NOTA: O builder object é 0x81C bytes (não 0x814!).
// addData é thiscall com ret 0x04 — cada chamada consome 1 arg da pilha.
// Game format: C1 07 19 skillH skillL targetH targetL (7 bytes)
// São 4 addData calls: skillH, skillL, targetH, targetL.
// O jogo lê o targetIndex do global 0x081C03A4 e o skillId de [ebp-0x10].
// ============================================================================

typedef void(__thiscall* FnBuilderInit)(void* thisPtr);
typedef void(__thiscall* FnBuilderSetHeader)(void* thisPtr, int header, int opcode);
typedef void*(__thiscall* FnBuilderAddData)(void* thisPtr, int arg1, int arg2, int arg3);
typedef void(__stdcall* FnSkillEncryptSend)(void* thisPtr, int arg1, int arg2);

static volatile LONG g_ProcessingSkillQueue = 0;
static void ProcessSkillQueue() {
    // Guard de reentrancia (0x404D70 envia via send -> HookSend -> aqui)
    if (InterlockedCompareExchange(&g_ProcessingSkillQueue, 1, 0) != 0) {
        return;
    }

    uintptr_t base = GameMemory::GetMainBase();
    if (!base) {
        InterlockedExchange(&g_ProcessingSkillQueue, 0);
        return;
    }

    while (true) {
        EnterCriticalSection(&g_SkillQueueLock);
        if (g_SkillQueue.empty()) {
            LeaveCriticalSection(&g_SkillQueueLock);
            break;
        }
        SkillPacketEntry entry = g_SkillQueue.front();
        g_SkillQueue.erase(g_SkillQueue.begin());
        LeaveCriticalSection(&g_SkillQueueLock);

        // Log do plaintext que estamos tentando enviar
        char hex[64];
        int h = 0;
        for (int i = 0; i < entry.len && i < 8; i++)
            h += snprintf(hex + h, sizeof(hex) - h, "%02X ", entry.data[i]);
        Logger::Log("PACKETSNIFFER", "SkillQueue: processing entry [%s] len=%d type=%d",
                    hex, entry.len, entry.type);

        if (entry.type == SKILL_TYPE_BUILDER) {
            // ============================================================
            // BUILDER PATH: Chama as funções nativas do jogo via inline
            // assembly para construir o pacote de skill attack.
            //
            // CALLING CONVENTION (confirmada via call stack x64dbg):
            //
            // 0x4050D0 (addData) é thiscall com ECX=this e 1 stack arg
            // consumido por ret 0x04. O jogo empilha 4 args e chama
            // 4 VEZES encadeadas, cada chamada consome 1 arg da pilha:
            //
            //   push targetL       ; arg4 (target low byte)
            //   push targetH       ; arg3 (target high byte)
            //   push skillL        ; arg2 (skill low byte)
            //   push skillH        ; arg1 (skill high byte)
            //   lea ecx,[builder]
            //   call 0x4050D0      ; consome skillH, ret 4
            //   mov ecx, eax       ; ECX = builder (return = this)
            //   call 0x4050D0      ; consome skillL, ret 4
            //   mov ecx, eax
            //   call 0x4050D0      ; consome targetH, ret 4
            //   mov ecx, eax
            //   call 0x4050D0      ; consome targetL, ret 4
            //   ; pilha equilibrada (16 bytes empilhados, 16 limpos)
            //
            // O jogo lê o targetIndex do global 0x081C03A4 e o skillId
            // do parâmetro [ebp-0x10] da função chamadora.
            //
            // IMPORTANTE: O builder object é 0x81C bytes (não 0x814!).
            // ============================================================

            Logger::Log("PACKETSNIFFER", "SkillQueue: BUILDER path — skillId=0x%04X attackId=0x%04X",
                        entry.skillId, entry.attackId);

            // Aloca builder na stack (0x81C bytes) com alinhamento
            // IMPORTANTE: O jogo usa 0x81C, não 0x814!
            __declspec(align(4)) uint8_t builderBuf[0x81C];
            memset(builderBuf, 0, sizeof(builderBuf));

            // Endereços absolutos das funções do jogo
            uintptr_t fnInit      = base + GameAddresses::SKILL_BUILDER_INIT;       // 0x404B40
            uintptr_t fnSetHeader  = base + GameAddresses::SKILL_BUILDER_SET_HEADER;  // 0x404B90
            uintptr_t fnAddData   = base + GameAddresses::SKILL_BUILDER_ADD_DATA;    // 0x4050D0
            uintptr_t fnEncrypt   = base + GameAddresses::SKILL_ENCRYPT_SEND;        // 0x404D70
            uintptr_t fnDestroy   = base + GameAddresses::SKILL_BUILDER_DESTROY;     // 0x404B60

            // Passo 1: Init builder (thiscall, sem stack args)
            typedef void(__thiscall* FnInit)(void*);
            ((FnInit)fnInit)(builderBuf);
            Logger::Log("PACKETSNIFFER", "BUILDER: init done");

            // Dump builder após init
            {
                char hexBuf[256];
                int hp = 0;
                for (int i = 0; i < 64 && hp + 3 < (int)sizeof(hexBuf); ++i)
                    hp += snprintf(hexBuf + hp, sizeof(hexBuf) - hp, "%02X ", builderBuf[i]);
                Logger::Log("PACKETSNIFFER", "BUILDER: after init: [%s]", hexBuf);
            }

            // Passo 2: SetHeader(builder, 0xC1, 0x19)
            // thiscall(ECX=builder), 2 stack args: push opcode(0x19), push header(0xC1)
            {
                __asm {
                    push 0x19                  ; opcode
                    push 0xC1                  ; header
                    lea ecx, dword ptr [builderBuf]
                    call dword ptr [fnSetHeader]
                }
            }
            Logger::Log("PACKETSNIFFER", "BUILDER: setHeader(0xC1, 0x19) done");

            // Dump builder após setHeader
            {
                char hexBuf[256];
                int hp = 0;
                for (int i = 0; i < 64 && hp + 3 < (int)sizeof(hexBuf); ++i)
                    hp += snprintf(hexBuf + hp, sizeof(hexBuf) - hp, "%02X ", builderBuf[i]);
                Logger::Log("PACKETSNIFFER", "BUILDER: after setHeader: [%s]", hexBuf);
            }

            // Passo 2.5: NÃO escrever skillId/targetIndex manualmente no builder.
            // O jogo NÃO escreve diretamente nos offsets do builder — ele passa
            // os valores como argumentos nas chamadas encadeadas de addData.
            // Cada addData(ret 0x04) consome 1 byte da pilha e escreve no buffer.
            //
            // A função 0x4050D0 faz isto:
            //   push 0x01; push 0x01; lea eax,[ebp+0x08]; push eax;
            //   mov ecx,[this]; call 0x404C10 (memcpy com tamanho)
            // Ou seja, addData pega o byte da pilha e copia para o buffer.
            //
            // IMPORTANTE: entry.attackId contém o RAW entity index (sem XOR 0xEFF5)

            // Passo 3-6: addData encadeado (4 chamadas, 4 args empilhados)
            // Game format: C1 07 19 skillH skillL targetH targetL (7 bytes)
            // 4 addData calls: skillH, skillL, targetH, targetL
            // Cada addData consome 1 arg via ret 0x04 e retorna this.
            {
                uint32_t skillH = (uint32_t)((entry.skillId >> 8) & 0xFF);
                uint32_t skillL = (uint32_t)(entry.skillId & 0xFF);
                uint32_t targetH = (uint32_t)((entry.attackId >> 8) & 0xFF);
                uint32_t targetL = (uint32_t)(entry.attackId & 0xFF);

                Logger::Log("PACKETSNIFFER", "BUILDER: addData args: skillH=0x%02X skillL=0x%02X targetH=0x%02X targetL=0x%02X",
                            skillH, skillL, targetH, targetL);

                __asm {
                    // Empilha 4 args para as 4 chamadas encadeadas de addData
                    // Na convenção do jogo: push targetL, push targetH, push skillL, push skillH
                    push targetL               ; arg4 (target low byte)
                    push targetH               ; arg3 (target high byte)
                    push skillL                ; arg2 (skill low byte)
                    push skillH                ; arg1 (skill high byte)

                    // 1ª chamada: addData(builder, skillH)
                    lea ecx, dword ptr [builderBuf]
                    call dword ptr [fnAddData]    ; consome skillH, ret 4 limpa 4 bytes

                    // 2ª chamada: addData(builder, skillL) — chained
                    mov ecx, eax                  ; ECX = builder (return = this)
                    call dword ptr [fnAddData]    ; consome skillL, ret 4 limpa 4 bytes

                    // 3ª chamada: addData(builder, targetH) — chained
                    mov ecx, eax
                    call dword ptr [fnAddData]    ; consome targetH, ret 4 limpa 4 bytes

                    // 4ª chamada: addData(builder, targetL) — chained
                    mov ecx, eax
                    call dword ptr [fnAddData]    ; consome targetL, ret 4 limpa 4 bytes
                }
            }
            Logger::Log("PACKETSNIFFER", "BUILDER: addData chained calls done (skillH=0x%02X skillL=0x%02X targetH=0x%02X targetL=0x%02X)",
                        (entry.skillId >> 8) & 0xFF, entry.skillId & 0xFF,
                        (entry.attackId >> 8) & 0xFF, entry.attackId & 0xFF);

            // Dump builder após addData
            {
                char hexBuf[256];
                int hp = 0;
                uint16_t pktSize = *(uint16_t*)builderBuf;
                for (int i = 0; i < 64 && hp + 3 < (int)sizeof(hexBuf); ++i)
                    hp += snprintf(hexBuf + hp, sizeof(hexBuf) - hp, "%02X ", builderBuf[i]);
                Logger::Log("PACKETSNIFFER", "BUILDER: after addData: size=%d data=[%s]", (int)pktSize, hexBuf);
            }

            // Passo 6: Encrypt & Send (builder, 1, 0)
            // thiscall(ECX=builder), 2 stack args: push 0, push 1
            {
                __asm {
                    push 0                  ; arg2 = 0
                    push 1                  ; arg1 = 1
                    lea ecx, dword ptr [builderBuf]
                    call dword ptr [fnEncrypt]
                }
            }
            Logger::Log("PACKETSNIFFER", "BUILDER: encrypt&send done (skillId=0x%04X attackId=0x%04X)",
                        entry.skillId, entry.attackId);

            // Passo 7: Destroy builder (thiscall, sem stack args)
            typedef void(__thiscall* FnDestroy)(void*);
            ((FnDestroy)fnDestroy)(builderBuf);
            Logger::Log("PACKETSNIFFER", "BUILDER: destroy done");
        }
        else if (entry.type == SKILL_TYPE_MANUAL) {
            // ============================================================
            // MANUAL PATH: Forma C1 plaintext → SM encrypt → C3 → Layer-1 → send
            //
            // Em vez de usar as funções nativas do jogo (BUILDER path),
            // formamos o pacote manualmente:
            //   1. C1 payload (sem header): 19 skillH skillL targetH targetL (5 bytes)
            //   2. SM-encrypt o payload: 5 bytes → 8 (padded) → 11 bytes
            //   3. Montar C3: C3 [2+11=0x0D] [11 bytes SM]
            //   4. Layer-1 encrypt o C3 inteiro
            //   5. Enviar via InjectToServer (que faz Layer-1 + TrueSend)
            //
            // NOTA: InjectToServer aplica Layer-1 em todo o buffer,
            // então passamos o C3 com SM-encrypted payload (antes de Layer-1).
            // ============================================================

            Logger::Log("PACKETSNIFFER", "MANUAL path — skillId=0x%04X attackId=0x%04X",
                        entry.skillId, entry.attackId);

            // Formar SM input = [decKey] [C1 payload]
            //
            // O jogo SM-encrypta 6 bytes: [decKey] [19] [skillH] [skillL] [targetH] [targetL]
            // Evidência (log do jogo): C3 decrypta para [2C] [19 E7 C3 E3 24] (6 bytes),
            // onde 2C é o decKey (stream[0], não faz parte do payload) e
            // 19 E7 C3 E3 24 é o C1 payload (5 bytes).
            //
            // O servidor SM-decrypta, pula o stream[0] (decKey) e usa stream[1..] como
            // payload do C1. Sem o decKey, o payload desloca 1 byte e o sub-opcode
            // fica 0xED (inválido) → servidor rejeita o pacote.
            //
            // entry.data = [C1] [07] [19] [skillH] [skillL] [targetH] [targetL]
            // C1 payload (sem C1+size) = entry.data + 2 = [19] [skillH] [skillL] [targetH] [targetL] (5 bytes)
            // SM input = [decKey] + C1 payload = 6 bytes
            //
            // O decKey (stream[0]) é o MAIN_PACKET_SERIAL do cliente — um contador
            // de 1 byte que o jogo incrementa a cada C3 enviado (DataSend, Protocol.cpp:504:
            //   EncBuff[1] = (*(BYTE*)MAIN_PACKET_SERIAL)++;  MAIN_PACKET_SERIAL = 0x08793700).
            // O servidor valida esse serial contra o valor esperado; se não bater, rejeita.
            //
            // Antes usávamos um `static g_decKey = 0` LOCAL, fora de sincronia com o serial
            // real do jogo (que já estava em 0x30 no log) → servidor rejeitava todos os
            // pacotes de skill injetados. Agora lemos o serial real do cliente, usamos como
            // decKey e incrementamos — assim o próximo C3 do jogo continua na sequência.
            uint8_t* pSerial = (uint8_t*)0x08793700;            // MAIN_PACKET_SERIAL (abs)
            uint8_t smInput[8];
            int c1PayloadLen = entry.len - 2;  // C1 payload sem C1+size = 5 bytes
            if (c1PayloadLen <= 0 || c1PayloadLen > 6) {
                Logger::Log("PACKETSNIFFER", "MANUAL: invalid payload len=%d (entry.len=%d)", c1PayloadLen, entry.len);
                continue;
            }
            smInput[0] = *pSerial;                               // decKey = serial real do cliente
            memcpy(smInput + 1, entry.data + 2, c1PayloadLen);  // C1 payload (stream[1..])
            (*pSerial)++;                                        // consome o serial (jogo continua em sync)
            int payloadLen = c1PayloadLen + 1;                  // inclui o decKey = 6 bytes
            
            char payloadHex[32];
            int ph = 0;
            for (int i = 0; i < payloadLen && ph + 3 < (int)sizeof(payloadHex); i++)
                ph += snprintf(payloadHex + ph, sizeof(payloadHex) - ph, "%02X ", smInput[i]);
            Logger::Log("PACKETSNIFFER", "MANUAL: SM input [%s] len=%d (decKey=%02X)", payloadHex, payloadLen, smInput[0]);
            
            // SM-encrypt o payload (com decKey)
            uint8_t smOutput[64];
            int smLen = SimpleModulusCipher::Encrypt(smOutput, smInput, payloadLen);
            if (smLen <= 0) {
                Logger::Log("PACKETSNIFFER", "MANUAL: SM encrypt failed (smLen=%d)", smLen);
                continue;
            }

            char smHex[128];
            int sh = 0;
            for (int i = 0; i < smLen && sh + 3 < (int)sizeof(smHex); i++)
                sh += snprintf(smHex + sh, sizeof(smHex) - sh, "%02X ", smOutput[i]);
            Logger::Log("PACKETSNIFFER", "MANUAL: SM encrypted [%s] len=%d", smHex, smLen);

            // DIAGNOSTIC: compare our Encrypt vs game's NativeEncrypt on same plaintext
            {
                uint8_t nativeOut[64];
                int nativeLen = SimpleModulusCipher::NativeEncrypt(nativeOut, smInput, payloadLen);
                if (nativeLen > 0) {
                    char nHex[128]; int nh = 0;
                    for (int i = 0; i < nativeLen && nh + 3 < (int)sizeof(nHex); i++)
                        nh += snprintf(nHex + nh, sizeof(nHex) - nh, "%02X ", nativeOut[i]);
                    bool match = (nativeLen == smLen) && (memcmp(smOutput, nativeOut, smLen) == 0);
                    Logger::Log("PACKETSNIFFER", "MANUAL DIAG: NativeEncrypt [%s] len=%d MATCH=%s",
                                nHex, nativeLen, match ? "YES" : "NO");
                    // Decrypt native result to see full 8-byte block (incl. padding/nonce)
                    uint8_t nativeDec[8] = {0};
                    int ndLen = SimpleModulusCipher::Decrypt(nativeDec, nativeOut, nativeLen);
                    if (ndLen > 0) {
                        char dHex[32]; int dh = 0;
                        for (int i = 0; i < 8 && dh + 3 < (int)sizeof(dHex); i++)
                            dh += snprintf(dHex + dh, sizeof(dHex) - dh, "%02X ", nativeDec[i]);
                        Logger::Log("PACKETSNIFFER", "MANUAL DIAG: native decrypted 8B [%s] valid=%d", dHex, ndLen);
                    }
                    // Also decrypt OUR result to compare padding
                    uint8_t ourDec[8] = {0};
                    int odLen = SimpleModulusCipher::Decrypt(ourDec, smOutput, smLen);
                    if (odLen > 0) {
                        char oHex[32]; int oh = 0;
                        for (int i = 0; i < 8 && oh + 3 < (int)sizeof(oHex); i++)
                            oh += snprintf(oHex + oh, sizeof(oHex) - oh, "%02X ", ourDec[i]);
                        Logger::Log("PACKETSNIFFER", "MANUAL DIAG: our decrypted 8B [%s] valid=%d", oHex, odLen);
                    }
                } else {
                    Logger::Log("PACKETSNIFFER", "MANUAL DIAG: NativeEncrypt FAILED (len=%d)", nativeLen);
                }
            }

            // Montar C3: C3 [total_size] [SM data]
            int c3TotalSize = 2 + smLen;  // C3 header (2 bytes) + SM data
            if (c3TotalSize > 255) {
                Logger::Log("PACKETSNIFFER", "MANUAL: C3 too large (%d bytes)", c3TotalSize);
                continue;
            }

            uint8_t c3Buf[256];
            int c3Pos = 0;
            c3Buf[c3Pos++] = 0xC3;                        // C3 header
            c3Buf[c3Pos++] = (uint8_t)c3TotalSize;        // size byte
            memcpy(c3Buf + c3Pos, smOutput, smLen);       // SM-encrypted payload
            c3Pos += smLen;

            Logger::Log("PACKETSNIFFER", "MANUAL: C3 packet [%02X %02X ...] total=%d bytes",
                        c3Buf[0], c3Buf[1], c3Pos);

            // Enviar via InjectToServer (aplica Layer-1 e TrueSend)
            if (!InjectToServer(c3Buf, c3Pos)) {
                Logger::Log("PACKETSNIFFER", "MANUAL: InjectToServer failed");
                continue;
            }

            Logger::Log("PACKETSNIFFER", "MANUAL: sent C3 packet (%d bytes) skillId=0x%04X attackId=0x%04X",
                        c3Pos, entry.skillId, entry.attackId);
        }
        else if (entry.type == SKILL_TYPE_STRUCT) {
            // ============================================================
            // STRUCT PATH: Monta o struct manualmente e passa para
            // 0x404D70. A função 0x404FA0 (chamada internamente)
            // corrige o size byte e encripta/envia.
            //
            // Formato: [word size][C1 xx F1 data...]
            // O byte[3] (size placeholder) é corrigido por 0x404FA0.
            // ============================================================
            uint8_t skillStruct[2 + 8] = {0};
            int entryLen = entry.len;
            memcpy(skillStruct + 2, entry.data, entryLen);
            *((uint16_t*)skillStruct) = (uint16_t)entryLen;

            uintptr_t funcAddr = base + GameAddresses::SKILL_ENCRYPT_SEND;
            __asm {
                lea eax, dword ptr [skillStruct]
                mov ecx, eax            ; ECX = &skillStruct (this)
                push 0                  ; arg2 = 0
                push 1                  ; arg1 = 1
                mov eax, dword ptr [funcAddr] ; SKILL_ENCRYPT_SEND
                call eax
            }

            Logger::Log("PACKETSNIFFER", "SkillQueue: sent via game 0x404D70 (STRUCT) [%s]", hex);
        }
        else {
            Logger::Log("PACKETSNIFFER", "SkillQueue: unknown type %d, skipping", entry.type);
        }
    }

    InterlockedExchange(&g_ProcessingSkillQueue, 0);
}

// ============================================================================
// InjectToServer — Injeta pacote plaintext no socket do jogo (mesma conexão TCP)
// O socket do jogo está conectado ao MuProxy2, que faz relay puro para o GS real.
// Cifra com Layer-1 (XOR+sub) e envia via TrueSend()
// ============================================================================
bool InjectToServer(const uint8_t* data, int len) {
    if (!data || len <= 0) return false;
    if (g_GameServerSocket == INVALID_SOCKET) {
        Logger::Log("PACKETSNIFFER", "InjectToServer: GS socket nao rastreado");
        return false;
    }

    // Verifica se o socket ainda esta vivo (nao foi fechado pelo jogo)
    int optval = 0;
    int optlen = sizeof(optval);
    if (getsockopt(g_GameServerSocket, SOL_SOCKET, SO_ERROR, (char*)&optval, &optlen) == SOCKET_ERROR || optval != 0) {
        Logger::Log("PACKETSNIFFER", "InjectToServer: socket invalido (err=%d), limpando", optval);
        g_GameServerSocket = INVALID_SOCKET;
        return false;
    }

    // Cifra: Camada 1 (Layer-1) — XOR+sub em todo o pacote.
    // ATENCAO: esta funcao recebe a forma ja processada pela Camada 2 (XorData)
    // nos pacotes C1 0x11 (ver InjectC1Attack). Para o plaintext REAL de um
    // ataque C1 0x11, use InjectC1Attack (que aplica XorData + Layer-1).
    uint8_t encrypted[4096];
    if (len > (int)sizeof(encrypted)) return false;
    memcpy(encrypted, data, len);
    GameServerFullEncrypt((char*)encrypted, len, 0x36, 0x07);

    // Envia no socket do jogo (vai para MuProxy2 → relay → GS real)
    int sent = TrueSend ? TrueSend(g_GameServerSocket, (const char*)encrypted, len, 0)
                        : send(g_GameServerSocket, (const char*)encrypted, len, 0);
    if (sent == SOCKET_ERROR) {
        int err = WSAGetLastError();
        Logger::Log("PACKETSNIFFER", "InjectToServer: send error %d - socket morto, limpando", err);
        g_GameServerSocket = INVALID_SOCKET;
        return false;
    }

    return true;
}

// ============================================================================
// InjectC1Attack — Injeta o PLAINTEXT REAL de um ataque C1 (0x11 normal /
// 0x19 skill). Aplica as DUAS camadas na ordem correta (VALIDADO, RE):
//   plaintext -> XorData[3,len) (Camada 2) -> Layer-1 (Camada 1) -> wire
// Ex. 0x11: C1 07 11 00 1A 78 04 -> C1 07 11 EF ED B6 77 -> FE 38 2E C0 C2 8B 48
// Ex. 0x19: C1 07 19 00 3C 02 77 -> (XorData) C1 07 19 E7 C3 E2 50 -> (Layer-1) wire
// Diferente de InjectToServer (que ja recebe a forma XorData e faz so Layer-1).
// ============================================================================
bool InjectC1Attack(const uint8_t* plaintext, int len) {
    if (!plaintext || len < 7) return false;
    if (plaintext[0] != 0xC1 || (plaintext[2] != 0x11 && plaintext[2] != 0x19)) return false;

    uint8_t buf[4096];
    if (len > (int)sizeof(buf)) return false;
    memcpy(buf, plaintext, len);

    // Camada 2 (XorData): XOR encadeado nos bytes de dado [3, len).
    // Append 0x404C10 -> XorData 0x404C90 (flag=1) no builder do jogo.
    GameServerXorData((char*)buf, 3, len);

    // Camada 1 (Layer-1) + send (delega para InjectToServer)
    return InjectToServer(buf, len);
}

// ============================================================================
// InjectSkillC1 — Injeta o PLAINTEXT REAL de um skill attack C1 0x19.
// Pipeline (diferente do 0x11, que vai C1 puro):
//   1. XorData[3,7)  — XOR encadeado no payload (key m_XorFilter, anchor 0x19)
//   2. SM input      — [serial 0x08793700] + payload XorData'd [19 sH sL iH iL]
//   3. SimpleModulus — 6 bytes → 11 bytes (1 bloco)
//   4. C3 0D [11]    — header C3 + size
//   5. Layer-1 + send
// O serial (MAIN_PACKET_SERIAL) é lido e incrementado — o próximo C3 do jogo
// continua na sequência.
// ============================================================================
bool InjectSkillC1(const uint8_t* plaintext, int len) {
    if (!plaintext || len != 7) return false;
    if (plaintext[0] != 0xC1 || plaintext[2] != 0x19) return false;

    // 1. XorData encadeado no payload [3,7) (anchor = byte 2 = 0x19)
    uint8_t buf[7];
    memcpy(buf, plaintext, 7);
    GameServerXorData((char*)buf, 3, 7);

    char xorHex[32];
    int xh = 0;
    for (int i = 0; i < 7; i++)
        xh += snprintf(xorHex + xh, sizeof(xorHex) - xh, "%02X ", buf[i]);
    Logger::Log("PACKETSNIFFER", "InjectSkillC1: after XorData [%s]", xorHex);

    // 2. SM input = [serial] + payload XorData'd (5 bytes: 19 sH sL iH iL)
    uint8_t* pSerial = (uint8_t*)0x08793700;            // MAIN_PACKET_SERIAL (abs)
    uint8_t smInput[6];
    smInput[0] = *pSerial;
    memcpy(smInput + 1, buf + 2, 5);
    (*pSerial)++;

    char smInHex[48];
    int ih = 0;
    for (int i = 0; i < 6; i++)
        ih += snprintf(smInHex + ih, sizeof(smInHex) - ih, "%02X ", smInput[i]);
    Logger::Log("PACKETSNIFFER", "InjectSkillC1: SM input [%s] (serial consumido)", smInHex);

    // 3. SimpleModulus: 6 bytes → 11 bytes
    uint8_t smOutput[64];
    int smLen = SimpleModulusCipher::Encrypt(smOutput, smInput, 6);
    if (smLen <= 0) {
        Logger::Log("PACKETSNIFFER", "InjectSkillC1: SM encrypt failed (smLen=%d)", smLen);
        return false;
    }

    char smHex[128];
    int sh = 0;
    for (int i = 0; i < smLen && sh + 3 < (int)sizeof(smHex); i++)
        sh += snprintf(smHex + sh, sizeof(smHex) - sh, "%02X ", smOutput[i]);
    Logger::Log("PACKETSNIFFER", "InjectSkillC1: SM encrypted [%s] len=%d", smHex, smLen);

    // 4. C3 0D [11 bytes]
    uint8_t c3Buf[2 + 64];
    c3Buf[0] = 0xC3;
    c3Buf[1] = (uint8_t)(2 + smLen);
    memcpy(c3Buf + 2, smOutput, smLen);

    // 5. Layer-1 + send
    if (!InjectToServer(c3Buf, 2 + smLen)) {
        Logger::Log("PACKETSNIFFER", "InjectSkillC1: InjectToServer failed");
        return false;
    }

    Logger::Log("PACKETSNIFFER", "InjectSkillC1: sent C3 0D (%d bytes) skill=0x%04X index=0x%04X",
                2 + smLen, (plaintext[3] << 8) | plaintext[4], (plaintext[5] << 8) | plaintext[6]);
    return true;
}

} // namespace PacketSniffer
