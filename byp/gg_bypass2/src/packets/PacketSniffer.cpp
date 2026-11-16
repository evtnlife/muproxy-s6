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
#include "ViewportMap.h"
#include "SkillMap.h"
#include "PacketReassembler.h"
#include "NameRewriter.h"
#include "PacketInjector.h"
#include "PacketChannel.h"
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
            if (opcode == (BYTE)MuOpcodes::ServerToClient::ObjectList) {
                ViewportMap::OnViewportPacket((const uint8_t*)outBuf, copyLen);
            } else if (opcode == (BYTE)MuOpcodes::ServerToClient::SkillList && copyLen > hdr + 1) {
                BYTE sub = (BYTE)outBuf[hdr + 1];
                if (sub == 0x11 || sub == 0xE2) {
                    SkillMap::OnSkillListPacket((const uint8_t*)outBuf, copyLen);
                }
            }
        }
    }

    // Auto-potion: o JOGADOR usou um item (C1 06 26). O módulo de potion
    // decodifica o payload (Camada-2 inversa) e guarda o modelo para replay.
    // Só na direção de SAÍDA (o que o cliente mandou).
    if (c->dir && c->dir[0] == 'S' && outBuf[0] == (BYTE)MuOpcodes::HeaderType::C1
        && copyLen >= 6 && outBuf[2] == (BYTE)MuOpcodes::ClientToServer::ItemUse) {
        AutoPotion::OnItemUsePacket((const uint8_t*)outBuf, 6);
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

    // O buffer ja esta decifrado/claro (isGameServer=false no consumer).
    ReasmContext ctx = { dir, xorKey, subKey, false };
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

    // Os hooks entregam os pacotes capturados ao LogRaw.
    WinsockHooks::SetPacketObserver(LogRaw);
    WinsockHooks::SetSocketClosedObserver(OnSocketClosed);

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
    g_Initialized = false;
}

} // namespace PacketSniffer
