// core/StackTrace.cpp — Stack trace manual via EBP walk (x86)
#include "StackTrace.h"
#include "Logger.h"

#include <cstdio>
#include <cstring>

namespace StackTrace {

void Log(const char* prefix, uintptr_t* pEbp) {
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

} // namespace StackTrace
