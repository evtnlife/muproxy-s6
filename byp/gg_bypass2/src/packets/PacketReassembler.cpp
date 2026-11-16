// PacketReassembler.cpp — Reassemble de stream TCP em pacotes MU Online
//
// Extraído do PacketSniffer (Fase 3 do refactor reorganize-byp-modules).
#include "PacketReassembler.h"
#include "MuCipher.h"

#include <map>
#include <vector>

namespace PacketReassembler {

static CRITICAL_SECTION g_Lock;
static bool g_Init = false;
static std::map<SOCKET, std::vector<BYTE>> g_Buffers;

static void EnsureInit()
{
    if (!g_Init) {
        InitializeCriticalSection(&g_Lock);
        g_Init = true;
    }
}

int ReadMuPacketLength(const uint8_t* buf, int len)
{
    if (!buf || len <= 0) return 0;
    BYTE h = buf[0];
    int hdr = MuCipher::GetMuHeaderSize(h);
    if (len < hdr) return 0;
    if (h == 0xC1 || h == 0xC3) return buf[1];
    if (h == 0xC2 || h == 0xC4) return (buf[1] << 8) | buf[2];
    return 0;
}

void Append(SOCKET s, const uint8_t* data, int len)
{
    if (!data || len <= 0) return;
    EnsureInit();
    EnterCriticalSection(&g_Lock);
    std::vector<BYTE>& buf = g_Buffers[s];
    buf.insert(buf.end(), data, data + len);
    LeaveCriticalSection(&g_Lock);
}

int Flush(SOCKET s, PacketCallback cb, void* ctx)
{
    if (!cb) return 0;
    EnsureInit();

    // Extrai os pacotes completos sob o lock e só então chama o consumer
    // (o consumer envia dados para o proxy — não deve segurar o lock).
    std::vector<std::vector<BYTE>> packets;
    int emitted = 0;

    EnterCriticalSection(&g_Lock);
    auto it = g_Buffers.find(s);
    if (it == g_Buffers.end()) {
        LeaveCriticalSection(&g_Lock);
        return 0;
    }

    std::vector<BYTE>& buf = it->second;
    while (!buf.empty())
    {
        int pktLen = ReadMuPacketLength(buf.data(), (int)buf.size());
        if (pktLen == 0 || (int)buf.size() < pktLen) break;
        // Sanity: pacote nao pode ser maior que buffer acumulado excessivo
        if (pktLen > 4096) {
            g_Buffers.erase(it);
            it = g_Buffers.end();
            break;
        }

        packets.push_back(std::vector<BYTE>(buf.begin(), buf.begin() + pktLen));
        buf.erase(buf.begin(), buf.begin() + pktLen);
        emitted++;
    }
    // Evita vazamento infinito se bytes lixo/nao-MU acumularem
    if (it != g_Buffers.end() && it->second.size() > 32768) {
        it->second.clear();
    }
    LeaveCriticalSection(&g_Lock);

    for (size_t i = 0; i < packets.size(); ++i) {
        cb(ctx, packets[i].data(), (int)packets[i].size());
    }
    return emitted;
}

void Remove(SOCKET s)
{
    if (!g_Init) return;
    EnterCriticalSection(&g_Lock);
    g_Buffers.erase(s);
    LeaveCriticalSection(&g_Lock);
}

} // namespace PacketReassembler
