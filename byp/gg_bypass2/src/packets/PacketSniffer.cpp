#include "sdk.h"
#include <cstdio>
#include <cstring>
#include <map>
#include <vector>

#include "PacketSniffer.h"
#include "Logger.h"
#include "GameMemory.h"
#include "MuCipher.h"
#include "SimpleModulusCipher.h"
#include "ViewportMap.h"
#include "SkillMap.h"
#include "PacketReassembler.h"
#include "NameRewriter.h"
#include "PacketInjector.h"
#include "PacketChannel.h"
#include "CommandChannel.h"
#include "OriginalApi.h"

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

static bool g_Initialized = false;

// Socket do game server do JOGO (conectado ao MuProxy2 → relay → GS real)
// Usamos este socket para injeção porque é a MESMA conexão TCP autenticada
static SOCKET g_GameServerSocket = INVALID_SOCKET;

// Transporte da injeção: envia bytes JÁ CIFRADOS no socket do GS.
// Registrado no PacketInjector (SetSender) no Install().
static bool SendToGameServer(const uint8_t* data, int len);

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

// Sockets internos do byp (canais TCP com o MuProxy2): o hook de log nao
// deve processar o trafego deles (sao linhas ASCII, nao pacotes MU).
static bool IsInternalSocket(SOCKET s)
{
    return PacketChannel::IsChannelSocket(s) || CommandChannel::IsChannelSocket(s);
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

// ============================================================================
// Consumer do PacketReassembler: recebe um pacote MU completo (ja decifrado)
// e faz o que o antigo FlushReasmBuffer fazia: envia "PKT|dir|hex" ao proxy.
// ============================================================================
struct ReasmContext {
    const char* dir;
    unsigned char xorKey;
    unsigned char subKey;
    bool isGameServer;
};

static void OnReassembledPacket(void* ctx, const uint8_t* data, int len)
{
    ReasmContext* c = (ReasmContext*)ctx;
    if (!c || !data || len <= 0) return;

    char outBuf[4096];
    int copyLen = len < (int)sizeof(outBuf) ? len : (int)sizeof(outBuf);
    memcpy(outBuf, data, copyLen);

    if (c->isGameServer)
        MuCipher::FullDecrypt(outBuf, copyLen, c->xorKey, c->subKey);

    // Parse de pacotes RECV (já decifrados) para alimentar os mapas dinâmicos.
    // Só processa tráfego do game server (porta 55902).
    if (c->isGameServer && c->dir && c->dir[0] == 'R') {
        int hdr = MuCipher::GetMuHeaderSize((BYTE)outBuf[0]);
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
    int pos = snprintf(line, sizeof(line), "PKT|%s|", c->dir);
    if (pos <= 0 || pos >= (int)sizeof(line)) return;
    for (int i = 0; i < copyLen && pos + 2 < (int)sizeof(line); ++i)
        pos += snprintf(line + pos, sizeof(line) - pos, "%02X", (unsigned char)outBuf[i]);
    if (pos + 1 < (int)sizeof(line))
        line[pos++] = '\n';
    else
        return;
    PacketChannel::SendLine(line, pos);
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
    if (IsInternalSocket(s)) return;

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
    if (isGameServer) MuCipher::GetGameServerKeys(&xorKey, &subKey);

    // Decifra pacotes do game server para logar em claro
    char decrypted[4096];
    const char* src = data;
    int srcLen = len;
    if (isGameServer) {
        int copyLen = len < (int)sizeof(decrypted) ? len : (int)sizeof(decrypted);
        memcpy(decrypted, data, copyLen);
        MuCipher::FullDecrypt(decrypted, copyLen, xorKey, subKey);
        src = decrypted;
        srcLen = copyLen;
    }

    // Nao loga hex bruto aqui (poluicao). O log detalhado (stack trace, C3/C4
    // decrypt, SMOD) e feito no HookSend/HookWSASend. So reassemble + proxy.

    // O buffer ja esta decifrado/claro (isGameServer=false no consumer).
    ReasmContext ctx = { dir, xorKey, subKey, false };
    PacketReassembler::Append(s, (const uint8_t*)src, srcLen);
    PacketReassembler::Flush(s, OnReassembledPacket, &ctx);
}

// ============================================================================
// Character Creation Rewrite: nome/classe no pacote 0x31
// (EncodeName / IsCreateCharPacket / g_NameKey / g_RewriteName / g_RewriteClass)
// vivem no módulo NameRewriter.
// ============================================================================

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
        // Aplica MuCipher::FullDecrypt (apenas Layer-1, sem XorFilter) em uma copia.
        int port = 0;
        bool isGS = GetSocketPort(s, &port) && IsGameServerPort(port);
        char decBuf[4096];
        const char* displayBuf = buf;
        int displayLen = len;
        if (isGS) {
            int copyLen = len < (int)sizeof(decBuf) ? len : (int)sizeof(decBuf);
            memcpy(decBuf, buf, copyLen);
            unsigned char xorKey = 0, subKey = 0;
            MuCipher::GetGameServerKeys(&xorKey, &subKey);
            MuCipher::FullDecrypt(decBuf, copyLen, xorKey, subKey);
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
                    //     de ataque C1 0x11 (ver PacketInjector::Inject / RE validada).
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

    return TrueSend(s, buf, len, flags);
}

static int WINAPI HookRecv(SOCKET s, char* buf, int len, int flags)
{
    int r = TrueRecv(s, buf, len, flags);
    if (r > 0) {
        Logger::Log("PACKETSNIFFER", "recv() ret=%d socket=%d", r, (int)s);
        LogRaw("RECV", s, buf, r);
    }
    if (r == 0) PacketReassembler::Remove(s); // conexao fechada
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
        PacketReassembler::Remove(s);
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
    g_Initialized = true;

    // O módulo de injeção usa este módulo como transporte (socket do GS).
    PacketInjector::SetSender(SendToGameServer);

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

    // Expõe os trampolines (send/connect originais) para os canais TCP
    // internos, que não devem passar pelo hook.
    OriginalApi::Set(TrueSend, TrueConnect);

    // O canal de pacotes (44413) sobe depois dos hooks: assim o connect já
    // usa o trampoline original.
    PacketChannel::Initialize();

    return g_HookCount > 0;
}

void InstallHooksForAllModules()
{
    Install();
}

void Uninstall()
{
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

    PacketChannel::Shutdown();

    g_GameServerSocket = INVALID_SOCKET;
    g_Initialized = false;
}

// ============================================================================
// SendToGameServer — envia bytes JÁ CIFRADOS no socket do game server.
// É o transporte do PacketInjector (o módulo dono do socket e do send
// original). O socket do jogo está conectado ao MuProxy2, que faz relay puro
// para o GS real.
// ============================================================================
static bool SendToGameServer(const uint8_t* data, int len)
{
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

    int sent = TrueSend ? TrueSend(g_GameServerSocket, (const char*)data, len, 0)
                        : send(g_GameServerSocket, (const char*)data, len, 0);
    if (sent == SOCKET_ERROR) {
        int err = WSAGetLastError();
        Logger::Log("PACKETSNIFFER", "InjectToServer: send error %d - socket morto, limpando", err);
        g_GameServerSocket = INVALID_SOCKET;
        return false;
    }

    return true;
}

} // namespace PacketSniffer
