// ============================================================================
// GameMemory/ClientVitals — HP/MP do jogador local lidos do CLIENTE
//
// Ver include/GameMemory/ClientVitals.h para o motivo (o servidor BlackRock
// não manda o máximo de life/mana — só o valor atual).
// ============================================================================
#include "GameMemory/ClientVitals.h"
#include "GameMemory/Globals.h"
#include "SafeRead.h"
#include "Logger.h"

namespace GameMemory {
namespace Player {

namespace {

uintptr_t g_blockAddr = 0;    // endereço de ViewCurHP (DWORD)
int       g_candidates = 0;   // quantos blocos casaram com a assinatura

inline bool MatchHint(const unsigned* hints, int count, unsigned v)
{
    if (!hints || count <= 0) return false;
    for (int i = 0; i < count; i++) {
        if (hints[i] == v) return true;
    }
    return false;
}

} // namespace

bool ScanClientVitals(const ClientVitalsHints& hints)
{
    if (g_blockAddr) return true;

    const uintptr_t base = Globals::MainBase();
    if (!base) return false;

    // ---- Cabeçalho PE: percorre as seções graváveis do main.exe ----
    const DWORD e_lfanew = SafeRead::ReadDword((const void*)(base + 0x3C));
    if (!e_lfanew || e_lfanew > 0x1000) return false;

    const uintptr_t nt = base + e_lfanew;
    const WORD numSections = SafeRead::ReadWord((const void*)(nt + 6));
    const WORD optSize = SafeRead::ReadWord((const void*)(nt + 20));
    if (!numSections || numSections > 96) return false;

    const uintptr_t secTable = nt + 24 + optSize;

    int found = 0;
    uintptr_t foundAddr = 0;
    unsigned fMaxLife = 0, fMaxMana = 0;

    for (int i = 0; i < numSections; i++) {
        const uintptr_t sec = secTable + (uintptr_t)i * 40;
        const DWORD flags = SafeRead::ReadDword((const void*)(sec + 36));
        if (!(flags & 0x80000000)) continue;          // IMAGE_SCN_MEM_WRITE

        const DWORD vsize = SafeRead::ReadDword((const void*)(sec + 8));
        const DWORD vaddr = SafeRead::ReadDword((const void*)(sec + 12));
        if (!vsize || vsize > 0x2000000) continue;

        const uintptr_t start = base + vaddr;
        const uintptr_t end = start + vsize;

        for (uintptr_t p = start; p + 32 <= end; p += 4) {
            // Só lê páginas verificadas (o .bss pode ter trechos não mapeados).
            if ((p & 0xFFF) == 0 && !SafeRead::IsReadable((const void*)p, 32)) {
                p += 0x1000 - 4;      // pula a página inteira
                continue;
            }
            if (!SafeRead::IsReadable((const void*)(p + 28), 4)) continue;

            const DWORD b0 = *(const DWORD*)(p + 0);   // ViewCurHP
            const DWORD b1 = *(const DWORD*)(p + 4);   // ViewMaxHP
            const DWORD b2 = *(const DWORD*)(p + 8);   // ViewCurMP
            const DWORD b3 = *(const DWORD*)(p + 12);  // ViewMaxMP
            const DWORD b4 = *(const DWORD*)(p + 16);  // ViewCurBP
            const DWORD b6 = *(const DWORD*)(p + 24);  // ViewCurSD

            if (b0 == 0 || b1 <= b0 || b1 > 100000) continue;
            if (b3 <= b2 || b3 > 100000) continue;
            if (!MatchHint(hints.life, hints.lifeCount, b0)) continue;
            if (!MatchHint(hints.mana, hints.manaCount, b2)) continue;
            if (!MatchHint(hints.bp, hints.bpCount, b4)) continue;
            if (!MatchHint(hints.shield, hints.shieldCount, b6)) continue;

            found++;
            if (!foundAddr) {
                foundAddr = p;
                fMaxLife = b1;
                fMaxMana = b3;
            }
            if (found >= 8) break;
        }
        if (found >= 8) break;
    }

    if (!foundAddr) return false;

    g_blockAddr = foundAddr;
    g_candidates = found;
    Logger::Log("GAMEMEMORY", "ClientVitals: bloco ViewCurHP/ViewMaxHP/ViewCurMP/ViewMaxMP "
                               "em %p (maxLife=%u maxMana=%u) — %d candidato(s)",
                (void*)foundAddr, fMaxLife, fMaxMana, found);
    return true;
}

bool ReadClientVitals(unsigned& curLife, unsigned& maxLife,
                       unsigned& curMana, unsigned& maxMana)
{
    if (!g_blockAddr) return false;
    if (!SafeRead::IsReadable((const void*)g_blockAddr, 16)) return false;

    curLife = *(const DWORD*)(g_blockAddr + 0);
    maxLife = *(const DWORD*)(g_blockAddr + 4);
    curMana = *(const DWORD*)(g_blockAddr + 8);
    maxMana = *(const DWORD*)(g_blockAddr + 12);
    return maxLife > 0 && maxMana > 0;
}

bool HasClientVitals()
{
    return g_blockAddr != 0;
}

void ResetClientVitals()
{
    g_blockAddr = 0;
    g_candidates = 0;
}

} // namespace Player
} // namespace GameMemory
