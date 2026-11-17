// client/ClientHooks.cpp — Hook de LoadLibraryA: aplica os patches no cliente
// (Main.dll) no instante em que ele é carregado.
#include "sdk.h"
#include <cstring>

#include "ClientHooks.h"
#include "ClientPatches.h"
#include "InlineHook.h"
#include "Logger.h"

namespace ClientHooks {

// ---------------------------------------------------------------------------
// Hook de LoadLibraryA
//
// O Main.dll é carregado dinamicamente pelo cliente. O patch da checagem de
// "instância única" (ClientPatches) depende dele estar pronto: sem isso o
// 2º cliente se mata sozinho (ExitProcess silencioso) ao enxergar a janela
// "Blackrock MU Online" do 1º.
//
// O hook fica aqui porque é um gatilho de "Main.dll carregado", compartilhado
// pelos patches do cliente.
// ---------------------------------------------------------------------------
typedef HMODULE (__stdcall *LoadLibraryAFn)(LPCSTR);

static LoadLibraryAFn g_RealLoadLibraryA = NULL;
static InlineHook g_LoadLibraryHook;

static bool IsMainDll(HMODULE h)
{
    if (!h) return false;
    char path[MAX_PATH] = {0};
    if (!GetModuleFileNameA(h, path, MAX_PATH)) return false;
    const char* name = path;
    for (const char* p = path; *p; ++p) {
        if (*p == '\\' || *p == '/') name = p + 1;
    }
    return _stricmp(name, "Main.dll") == 0;
}

static HMODULE __stdcall HookLoadLibraryA(LPCSTR fileName)
{
    HMODULE h = g_RealLoadLibraryA ? g_RealLoadLibraryA(fileName) : NULL;
    if (IsMainDll(h)) {
        Logger::Log("CLIENT", "Main.dll carregado pelo cliente — aplicando os patches");
        ClientPatches::Apply((uintptr_t)h);
        ClientPatches::ApplyMutexBypass((uintptr_t)h);
    }
    return h;
}

void Initialize()
{
    HMODULE k32 = GetModuleHandleA("kernel32.dll");
    if (!k32) return;
    FARPROC p = GetProcAddress(k32, "LoadLibraryA");
    if (!p) return;

    g_RealLoadLibraryA = (LoadLibraryAFn)p;
    // Prologo do LoadLibraryA: mov edi,edi / push ebp / mov ebp,esp (5 bytes)
    if (!g_LoadLibraryHook.Install((void*)p, (void*)HookLoadLibraryA, 5)) {
        Logger::Log("CLIENT", "falha ao hookar LoadLibraryA");
        return;
    }
    g_RealLoadLibraryA = (LoadLibraryAFn)g_LoadLibraryHook.GetTrampoline();
    Logger::Log("CLIENT", "hook de LoadLibraryA instalado");
}

} // namespace ClientHooks
