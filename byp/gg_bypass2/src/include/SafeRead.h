#pragma once

#include <windows.h>
#include <cstddef>
#include <cstdint>

// ============================================================================
// SafeRead — leituras de memória do jogo sem derrubar o processo.
//
// ⚠️ POR QUE NÃO BASTA USAR __try/__except:
//   O compilador (MSVC /O2) pode MOVER um load para FORA da região protegida
//   por __try (ele assume que "carregar memória não lança exceção" — falso
//   para memória inválida). Resultado: o ACCESS_VIOLATION acontece fora do
//   try e mata o jogo. Foi exatamente o que matou o cliente no AutoPotion
//   (crash em RVA 0x81C9: "mov eax,[eax+14h]").
//
// A garantia aqui é EXPLÍCITA: primeiro checa-se se a página é legível
// (VirtualQuery) e só depois se lê. Sem load-speculation, sem load-hoisting,
// sem exceção.
//
// Uso:  WORD v = SafeReadWord(ptr + OFF_X);   // 0 se a memória não for legível
// ============================================================================

namespace SafeRead {

// A memória [p, p+len) é legível?
inline bool IsReadable(const void* p, size_t len)
{
    if (!p || len == 0) return false;

    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(p, &mbi, sizeof(mbi)) != sizeof(mbi)) return false;
    if (mbi.State != MEM_COMMIT) return false;
    if (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) return false;

    const BYTE* base = (const BYTE*)mbi.BaseAddress;
    const BYTE* end  = base + mbi.RegionSize;
    const BYTE* q    = (const BYTE*)p;
    if (q < base || q + len > end) return false;

    return true;
}

inline BYTE  ReadByte(const void* p)
{
    if (!IsReadable(p, 1)) return 0;
    return *(const BYTE*)p;
}

inline WORD  ReadWord(const void* p)
{
    if (!IsReadable(p, 2)) return 0;
    return *(const WORD*)p;
}

inline DWORD ReadDword(const void* p)
{
    if (!IsReadable(p, 4)) return 0;
    return *(const DWORD*)p;
}

inline int   ReadInt(const void* p)
{
    if (!IsReadable(p, 4)) return 0;
    return *(const int*)p;
}

inline uintptr_t ReadPointer(const void* p)
{
    if (!IsReadable(p, sizeof(uintptr_t))) return 0;
    return *(const uintptr_t*)p;
}

inline float ReadFloat(const void* p)
{
    if (!IsReadable(p, 4)) return 0.0f;
    return *(const float*)p;
}

// String ASCII (para validar nomes) — sem fault possível.
inline bool ReadAsciiName(const void* p, char* out, int maxLen)
{
    if (!p || !out || maxLen <= 0) return false;
    for (int i = 0; i < maxLen - 1; i++) {
        BYTE c = ReadByte((const BYTE*)p + i);
        if (c == 0) { out[i] = '\0'; return i > 0; }
        if (c < 0x20 || c > 0x7E) { out[i] = '\0'; return false; }
        out[i] = (char)c;
    }
    out[maxLen - 1] = '\0';
    return true;
}

} // namespace SafeRead
