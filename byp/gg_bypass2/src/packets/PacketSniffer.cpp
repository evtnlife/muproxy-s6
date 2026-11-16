// packets/PacketSniffer.cpp — Orquestrador da captura de pacotes
//
// O módulo em si não hooka nada: os hooks winsock vivem em WinsockHooks
// (src/net/winsock/WinsockHooks.cpp) e entregam os pacotes capturados aqui,
// via observador. Este módulo cuida do resto do fluxo:
//
//   hook -> LogRaw (decifra) -> PacketReassembler (stream -> pacotes MU)
//        -> "PKT|<dir>|<hex>" -> PacketChannel (44413) -> MuProxy2
//
// O tag de log continua "PACKETSNIFFER" para manter a continuidade dos logs.
#include "sdk.h"
#include <cstdio>
#include <cstring>

#include "PacketSniffer.h"
#include "Logger.h"
#include "MuCipher.h"
#include "MuOpcodes.h"
#include "C3Decoder.h"
#include "SimpleModulusCipher.h"
#include "ViewportMap.h"
#include "SkillMap.h"
#include "PacketReassembler.h"
#include "NameRewriter.h"
#include "PacketInjector.h"
#include "PacketChannel.h"
#include "PacketDiag.h"
#include "CommandChannel.h"
#include "GameServerConnection.h"
#include "WinsockHooks.h"
#include "AutoPotion.h"

#pragma comment(lib, "ws2_32.lib")

namespace PacketSniffer {

static bool g_Initialized = false;

// Sockets internos do byp (canais TCP com o MuProxy2): o hook de log nao
// deve processar o trafego deles (sao linhas ASCII, nao pacotes MU).
static bool IsInternalSocket(SOCKET s)
{
    return PacketChannel::IsChannelSocket(s) || CommandChannel::IsChannelSocket(s);
}

// ============================================================================
// Observador do C3Decoder: o pacote INTERNO (C1) de um C3+SimpleModulus.
//
// O cliente envia o uso de item (0x26) dentro de um C3+SM — é aqui (e só
// aqui) que ele aparece decodificado. O auto-potion usa isso para descobrir
// o slot da potion: "learn & replay".
// ============================================================================
static void OnInnerPacket(const uint8_t* inner, int len)
{
    if (!inner || len < 6) return;
    if (inner[0] != (uint8_t)MuOpcodes::HeaderType::C1) return;
    if (inner[2] != (uint8_t)MuOpcodes::ClientToServer::ItemUse) return;
    AutoPotion::OnItemUsePacket(inner, 6);
}

// ============================================================================
// Consumer do PacketReassembler: recebe um pacote MU completo (já decifrado)
// e faz o que o antigo FlushReasmBuffer fazia: envia "PKT|dir|hex" ao proxy.
// ============================================================================
struct ReasmContext {
    const char* dir;
    bool isGameServer;   // tráfego do GS — o buffer JÁ vem decifrado do LogRaw
    SOCKET socket;       // socket de origem (para achar o socket de gameplay)
};

static void OnReassembledPacket(void* ctx, const uint8_t* data, int len)
{
    ReasmContext* c = (ReasmContext*)ctx;
    if (!c || !data || len <= 0) return;

    char outBuf[4096];
    int copyLen = len < (int)sizeof(outBuf) ? len : (int)sizeof(outBuf);
    memcpy(outBuf, data, copyLen);

    // Diagnóstico em arquivo PRÓPRIO (sobrevive a limpeza do log principal).
    PacketDiag::LogPacket(c->dir, (const uint8_t*)outBuf, copyLen);
    {
        // Histograma de opcodes S→C a cada 5s — mostra qual opcode o
        // servidor realmente usa (ex.: o update de life pode não ser 0x26).
        static DWORD lastHist = 0;
        DWORD now = GetTickCount();
        if (lastHist == 0) lastHist = now;
        else if (now - lastHist >= 5000) {
            lastHist = now;
            PacketDiag::LogHistogram();
        }
    }

    // ⚠️ NÃO decifrar aqui: o LogRaw já entregou o buffer DECIFRADO (a
    // Camada-1 foi aplicada antes do reassembly). Decifrar de novo quebra o
    // header — o parse de S→C (viewport, skills, life/mana) não reconheceria
    // nenhum pacote.
    // Parse de pacotes RECV (já decifrados) para alimentar os mapas dinâmicos.
    // Só processa tráfego do game server.
    if (c->isGameServer && c->dir && c->dir[0] == 'R') {
        int hdr = MuCipher::GetMuHeaderSize((BYTE)outBuf[0]);
        if (hdr > 0 && copyLen > hdr) {
            BYTE opcode = (BYTE)outBuf[hdr];
            if (opcode == (BYTE)MuOpcodes::ServerToClient::ObjectList) {
                ViewportMap::OnViewportPacket((const uint8_t*)outBuf, copyLen);
            } else if (opcode == (BYTE)MuOpcodes::ServerToClient::SkillList && copyLen > hdr + 1) {
                BYTE sub = (BYTE)outBuf[hdr + 1];
                if (sub == 0x11 || sub == 0xE2) {
                    SkillMap::OnSkillListPacket((const uint8_t*)outBuf, copyLen);
                }
            }

            // Auto-potion: life/mana (C1 26/27), character info (F3 03) e
            // inventário (F3 10) — pacotes C3 são decodificados (SimpleModulus).
            AutoPotion::OnServerPacket((const uint8_t*)outBuf, copyLen);

            // O socket que traz objetos/life é onde o jogo está JOGANDO —
            // é por ele que a injeção tem que sair (o servidor processa o
            // pacote na sessão dessa conexão).
            if (opcode == (BYTE)MuOpcodes::ServerToClient::ObjectList ||
                opcode == (BYTE)MuOpcodes::ServerToClient::LifeUpdate ||
                opcode == (BYTE)MuOpcodes::ServerToClient::ManaUpdate) {
                GameServerConnection::SetGameplaySocket(c->socket);
            }
        }
    }

    // Diagnóstico: os primeiros pacotes S→C da sessão (para achar o de
    // inventário e os updates de life/mana no formato real do BlackRock).
    if (c->dir && c->dir[0] == 'R') {
        static int s2cLogged = 0;
        if (s2cLogged < 60) {
            s2cLogged++;
            int hdr = MuCipher::GetMuHeaderSize((BYTE)outBuf[0]);
            char hex[160];
            int hp = 0;
            int show = copyLen < 48 ? copyLen : 48;
            for (int i = 0; i < show && hp < (int)sizeof(hex) - 4; i++)
                hp += snprintf(hex + hp, sizeof(hex) - hp, "%02X ", (unsigned char)outBuf[i]);
            Logger::Log("PACKETSNIFFER", "S->C #%d: hdr=%d op=%02X len=%d [%s]",
                        s2cLogged, hdr, hdr > 0 ? (unsigned)(unsigned char)outBuf[hdr] : 0, copyLen, hex);
        }

        // Pacotes F3 (character info, inventário, skill list) — são os que
        // trazem os dados que o auto-potion precisa. Loga todos (até 30).
        int hdr2 = MuCipher::GetMuHeaderSize((BYTE)outBuf[0]);
        if (hdr2 > 0 && copyLen > hdr2 && (BYTE)outBuf[hdr2] == 0xF3) {
            static int f3Logged = 0;
            if (f3Logged < 30) {
                f3Logged++;
                char hex[200];
                int hp = 0;
                int show = copyLen < 64 ? copyLen : 64;
                for (int i = 0; i < show && hp < (int)sizeof(hex) - 4; i++)
                    hp += snprintf(hex + hp, sizeof(hex) - hp, "%02X ", (unsigned char)outBuf[i]);
                Logger::Log("PACKETSNIFFER", "S->C F3 #%d: sub=%02X len=%d [%s]",
                            f3Logged, copyLen > hdr2 + 1 ? (unsigned)outBuf[hdr2 + 1] : 0,
                            copyLen, hex);
            }
        }
    }

    // Auto-potion: o JOGADOR usou um item (0x26).
    //
    // O jogo envia o uso de item DENTRO de um C3+SM (captura 18:51:47:
    // C3 0D → inner C1 06 26 …) — esse caminho é tratado pelo observador
    // OnInnerPacket (o C3Decoder já decodifica o SM). Aqui fica só o caso
    // do C1 puro.
    if (c->dir && c->dir[0] == 'S' && copyLen >= 6) {
        if (outBuf[0] == (BYTE)MuOpcodes::HeaderType::C1 &&
            outBuf[2] == (BYTE)MuOpcodes::ClientToServer::ItemUse) {
            AutoPotion::OnItemUsePacket((const uint8_t*)outBuf, 6);
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
// (o log detalhado fica nos hooks). So decifra trafego do game server.
// ============================================================================
static void LogRaw(const char* dir, SOCKET s, const char* data, int len)
{
    if (len <= 0 || !data) return;

    // Pula os sockets internos do byp (canais TCP com o MuProxy2)
    if (IsInternalSocket(s)) return;

    // Pula mensagens de LOG (comecam com "LOG " = 0x4C 0x4F 0x47 0x20)
    if (len >= 4 && (unsigned char)data[0] == 0x4C && (unsigned char)data[1] == 0x4F
        && (unsigned char)data[2] == 0x47 && (unsigned char)data[3] == 0x20)
        return;

    // Detecta a porta do GS via C116 (ConnectToServer) no trafego RECV
    // (login server -> cliente). O pacote chega cifrado, entao escaneamos os bytes crus.
    if (dir && dir[0] == 'R')
        GameServerConnection::DetectGsPortFromC116(data, len);

    bool isGameServer = GameServerConnection::IsGameServerSocket(s);

    // Detecção do socket do GS: a PORTA não serve (o hook de connect
    // redireciona o jogo para 127.0.0.1/MuProxy2 e a porta real do GS pode
    // estar fora do range hardcoded) — o que identifica é o CONTEÚDO:
    // tráfego decifrado que começa com um header MU válido.
    if (!isGameServer && !IsInternalSocket(s)) {
        isGameServer = GameServerConnection::MarkIfGameServerSocket(s, (const uint8_t*)data, len);
    }

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
    // decrypt, SMOD) e feito nos hooks. So reassemble + proxy.

    // O buffer JÁ está decifrado (a Camada-1 foi aplicada no LogRaw). O flag
    // do contexto significa "isto é tráfego do game server" — e é ele que
    // habilita o parse dos pacotes S→C (viewport, skill list, life/mana).
    ReasmContext ctx = { dir, isGameServer, s };
    PacketReassembler::Append(s, (const uint8_t*)src, srcLen);
    PacketReassembler::Flush(s, OnReassembledPacket, &ctx);
}

// Socket encerrado: descarta o buffer de reassembly do socket.
static void OnSocketClosed(SOCKET s)
{
    PacketReassembler::Remove(s);
}

bool Install()
{
    if (g_Initialized) return true;
    g_Initialized = true;

    // Diagnóstico S→C em arquivo próprio, truncado a cada boot
    // (o log principal vive na pasta do jogo e às vezes é limpo).
    {
        HMODULE hMod = NULL;
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                               (LPCSTR)&PacketSniffer::Install, &hMod)) {
            char path[MAX_PATH] = { 0 };
            if (GetModuleFileNameA(hMod, path, MAX_PATH)) {
                char* slash = strrchr(path, '\\');
                if (slash) *slash = '\0';
                PacketDiag::Initialize(path);
            }
        }
    }

    // Os hooks entregam os pacotes capturados ao LogRaw.
    WinsockHooks::SetPacketObserver(LogRaw);
    WinsockHooks::SetSocketClosedObserver(OnSocketClosed);

    // Observa o pacote interno dos C3+SM (é onde aparece o uso de item 0x26).
    C3Decoder::SetInnerPacketObserver(OnInnerPacket);

    // Os hooks sao instalados SINCRONAMENTE (esta chamada vem do
    // DLL_PROCESS_ATTACH) para capturar o connect do jogo ao game server.
    if (!WinsockHooks::Install()) return false;

    // O módulo de injeção usa a conexão do GS como transporte (socket do GS).
    PacketInjector::SetSender(GameServerConnection::SendToGameServer);

    // O canal de pacotes (44413) sobe depois dos hooks: assim o connect já
    // usa o trampoline original.
    PacketChannel::Initialize();

    return true;
}

void InstallHooksForAllModules()
{
    Install();
}

void Uninstall()
{
    PacketChannel::Shutdown();
    WinsockHooks::Uninstall();
    PacketDiag::Shutdown();
    g_Initialized = false;
}

} // namespace PacketSniffer
