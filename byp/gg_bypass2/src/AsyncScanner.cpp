#include "include/AsyncScanner.h"
#include "include/Logger.h"
#include "include/PsBypass.h"
#include "include/sdk.h"
#include <string.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")

static HANDLE g_hThread = nullptr;
static volatile LONG g_Running = 0;

static bool IsAddressInModule(HMODULE hMod, uintptr_t address) {
    if (!hMod) return false;
    MODULEINFO mi = {};
    if (!GetModuleInformation(GetCurrentProcess(), hMod, &mi, sizeof(mi))) return false;
    uintptr_t base = (uintptr_t)mi.lpBaseOfDll;
    return (address >= base && address < base + mi.SizeOfImage);
}

static bool ScanBuffer(const BYTE* data, SIZE_T size, bool* unicode) {
    if (size < 9) return false;
    for (SIZE_T i = 0; i + 9 <= size; ++i) {
        if (data[i] == 'M' && memcmp(data + i, "Main.free", 9) == 0) {
            *unicode = false;
            return true;
        }
    }
    for (SIZE_T i = 0; i + 18 <= size; i += 2) {
        if (data[i] == 'M' && data[i+2] == 'a' && data[i+4] == 'i' && data[i+6] == 'n' &&
            data[i+8] == '.' && data[i+10] == 'f' && data[i+12] == 'r' && data[i+14] == 'e' && data[i+16] == 'e') {
            *unicode = true;
            return true;
        }
    }
    return false;
}

static DWORD WINAPI ScannerThread(LPVOID) {
    Logger::Log("SCANNER", "Thread de analise iniciada");

    // Aguarda o PS.dll carregar (timeout 15s).
    HMODULE hPs = nullptr;
    for (int i = 0; i < 150 && !hPs; ++i) {
        hPs = PsBypass::GetPsModule();
        if (!hPs) Sleep(100);
    }
    if (!hPs) {
        Logger::Log("SCANNER", "PS.dll nao carregou; encerrando scanner");
        return 0;
    }
    Logger::Log("SCANNER", "PS.dll encontrado em 0x%p", (void*)hPs);

    // Coleta regioes de memoria executaveis/legiveis fora dos modulos conhecidos
    // e dentro do PS.dll, e procura por Main.free.
    while (InterlockedCompareExchange(&g_Running, 1, 1) == 1) {
        MEMORY_BASIC_INFORMATION mbi = {};
        uintptr_t addr = 0;
        int scanned = 0;
        int found = 0;

        while (VirtualQuery((LPCVOID)addr, &mbi, sizeof(mbi)) == sizeof(mbi)) {
            bool readable = (mbi.State == MEM_COMMIT) &&
                            (mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE));

            if (readable && mbi.RegionSize > 0 && mbi.RegionSize <= 0x100000) {
                // Verifica se a regiao pertence ao PS.dll ou e executavel e fora de modulos conhecidos.
                bool inPs = IsAddressInModule(hPs, (uintptr_t)mbi.BaseAddress);
                bool executable = (mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) != 0;

                if (inPs || executable) {
                    BYTE* buf = (BYTE*)HeapAlloc(GetProcessHeap(), 0, mbi.RegionSize + 1);
                    if (buf) {
                        SIZE_T read = 0;
                        if (ReadProcessMemory(GetCurrentProcess(), mbi.BaseAddress, buf, mbi.RegionSize, &read) && read > 0) {
                            bool unicode = false;
                            if (ScanBuffer(buf, read, &unicode)) {
                                Logger::Log("SCANNER", "!!! Main.free %s em 0x%p (size=0x%llX, inPs=%d, exec=%d) !!!",
                                            unicode ? "UNICODE" : "ANSI", mbi.BaseAddress,
                                            (unsigned long long)mbi.RegionSize, inPs ? 1 : 0, executable ? 1 : 0);
                                found++;
                            }
                            scanned++;
                        }
                        HeapFree(GetProcessHeap(), 0, buf);
                    }
                }
            }

            uintptr_t next = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
            if (next <= addr) break; // seguranca
            addr = next;
        }

        Logger::Log("SCANNER", "Scan completo: %d regioes analisadas, %d com Main.free", scanned, found);

        // Repete a cada 3s por 5 iteracoes, depois para.
        static int iterations = 0;
        if (++iterations >= 5) break;
        Sleep(3000);
    }

    Logger::Log("SCANNER", "Thread de analise encerrada");
    return 0;
}

void AsyncScanner::Start() {
    if (g_hThread) return;
    InterlockedExchange(&g_Running, 1);
    g_hThread = CreateThread(nullptr, 0, ScannerThread, nullptr, 0, nullptr);
    if (!g_hThread) {
        InterlockedExchange(&g_Running, 0);
        Logger::Log("SCANNER", "FALHA ao criar thread de analise");
    }
}

void AsyncScanner::Stop() {
    InterlockedExchange(&g_Running, 0);
    if (g_hThread) {
        WaitForSingleObject(g_hThread, 3000);
        CloseHandle(g_hThread);
        g_hThread = nullptr;
    }
}
