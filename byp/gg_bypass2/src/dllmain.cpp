#include "include/sdk.h"
#include <Logger.h>
#include <Config.h>
#include <PacketSniffer.h>
#include <TcpClient.h>
#include <PacketHandler.h>
#include <MonsterTracker.h>
#include <AttackModule.h>
#include <SkillHook.h>

static HANDLE g_hInitThread = NULL;
static LPTOP_LEVEL_EXCEPTION_FILTER g_PrevExceptionFilter = NULL;
static PVOID g_VehHandle = NULL;

// ============================================================================
// Global exception handler — loga crashes de QUALQUER módulo (main.exe, Main.dll, byp.dll)
// ============================================================================
static void LogExceptionInfo(const char* source, EXCEPTION_POINTERS* ep) {
    if (!ep || !ep->ExceptionRecord || !ep->ContextRecord) {
        Logger::Log("CRASH", "%s: EXCEPTION com pointers nulos", source);
        return;
    }
    
    EXCEPTION_RECORD* er = ep->ExceptionRecord;
    CONTEXT* ctx = ep->ContextRecord;
    
    Logger::Log("CRASH", "=== EXCEPTION %s ===", source);
    Logger::Log("CRASH", "  Code: 0x%08X Flags: 0x%08X Addr: 0x%08X",
        er->ExceptionCode, er->ExceptionFlags, (uintptr_t)er->ExceptionAddress);
    
    // Identifica o módulo onde ocorreu
    HMODULE hMod = NULL;
    GetModuleHandleA(NULL); // force load
    char modName[256] = "unknown";
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, 
                           (LPCSTR)er->ExceptionAddress, &hMod)) {
        GetModuleFileNameA(hMod, modName, sizeof(modName));
    }
    Logger::Log("CRASH", "  Module: %s", modName);
    
    // Registros x86
    Logger::Log("CRASH", "  EAX=0x%08X EBX=0x%08X ECX=0x%08X EDX=0x%08X",
        ctx->Eax, ctx->Ebx, ctx->Ecx, ctx->Edx);
    Logger::Log("CRASH", "  ESI=0x%08X EDI=0x%08X EBP=0x%08X ESP=0x%08X",
        ctx->Esi, ctx->Edi, ctx->Ebp, ctx->Esp);
    Logger::Log("CRASH", "  EIP=0x%08X EFL=0x%08X",
        ctx->Eip, ctx->EFlags);
    
    // Stack dump (16 DWORDs a partir de ESP)
    __try {
        DWORD* sp = (DWORD*)ctx->Esp;
        Logger::Log("CRASH", "  Stack: %08X %08X %08X %08X %08X %08X %08X %08X",
            sp[0], sp[1], sp[2], sp[3], sp[4], sp[5], sp[6], sp[7]);
        Logger::Log("CRASH", "         %08X %08X %08X %08X %08X %08X %08X %08X",
            sp[8], sp[9], sp[10], sp[11], sp[12], sp[13], sp[14], sp[15]);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        Logger::Log("CRASH", "  Stack: unable to read");
    }
    
    // Exception info
    if (er->NumberParameters > 0) {
        Logger::Log("CRASH", "  Params: %d", er->NumberParameters);
        for (DWORD i = 0; i < er->NumberParameters && i < EXCEPTION_MAXIMUM_PARAMETERS; i++) {
            Logger::Log("CRASH", "    [%d] 0x%08X", i, er->ExceptionInformation[i]);
        }
    }
    
    Logger::Log("CRASH", "=== END EXCEPTION ===");
    Logger::Shutdown(); // flush log antes de morrer
}

// Unhandled Exception Filter — captura crashes NÃO tratados por __try/__except
static LONG WINAPI CrashExceptionFilter(EXCEPTION_POINTERS* ep) {
    LogExceptionInfo("Unhandled", ep);
    if (g_PrevExceptionFilter) return g_PrevExceptionFilter(ep);
    return EXCEPTION_CONTINUE_SEARCH;
}

// Vectored Exception Handler — captura FIRST-CHANCE exceptions
// SKIPS stack overflow (0xC00000FD) since logging it would recurse
static LONG WINAPI VectoredCrashHandler(EXCEPTION_POINTERS* ep) {
    if (ep && ep->ExceptionRecord) {
        DWORD code = ep->ExceptionRecord->ExceptionCode;
        // SKIP stack overflow — logging would cause another stack overflow → crash
        if (code == EXCEPTION_STACK_OVERFLOW) {
            return EXCEPTION_CONTINUE_SEARCH;
        }
        // Log only hard crashes: access violation, illegal instruction, etc.
        // Skip FLT and INT_OVERFLOW — those are normal first-chance exceptions
        if (code == EXCEPTION_ACCESS_VIOLATION ||
            code == EXCEPTION_INT_DIVIDE_BY_ZERO || code == EXCEPTION_ILLEGAL_INSTRUCTION ||
            code == EXCEPTION_PRIV_INSTRUCTION || code == EXCEPTION_DATATYPE_MISALIGNMENT ||
            code == EXCEPTION_ARRAY_BOUNDS_EXCEEDED) {
            LogExceptionInfo("FirstChance", ep);
        }
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

extern "C" __declspec(dllexport) void DummyFunc() {}

static void InitializeLogging(HMODULE hModule) {
    char logPath[MAX_PATH] = {0};
    char pktPath[MAX_PATH] = {0};
    if (GetModuleFileNameA(hModule, logPath, MAX_PATH)) {
        char* lastSlash = strrchr(logPath, '\\');
        if (lastSlash) {
            *(lastSlash + 1) = '\0';
            strcpy_s(pktPath, sizeof(pktPath), logPath);
            strcat_s(logPath, sizeof(logPath), Config::LOG_FILENAME);
            strcat_s(pktPath, sizeof(pktPath), Config::PACKET_LOG_FILENAME);
        }
    }
    if (logPath[0] == '\0') {
        strcpy_s(logPath, sizeof(logPath), Config::LOG_FILENAME);
        strcpy_s(pktPath, sizeof(pktPath), Config::PACKET_LOG_FILENAME);
    }
    Logger::Initialize(logPath, pktPath);
}

static void SafeInitialize(HMODULE hModule) {
    Logger::Log("SYSTEM", "=== GG Hook DLL Injected ===");
    Logger::Log("SYSTEM", "Versao: %s | Build: %s", Config::VERSION, Config::BUILD);

    if (!Config::BYPASS_ENABLED) {
        Logger::Log("SYSTEM", "BYPASS_ENABLED = false: nenhum hook/bypass sera aplicado.");
        return;
    }

    Logger::Log("SYSTEM", "BYPASS_ENABLED = true: instalando hooks.");

    // Inicializa TCP client e módulos de comunicação com MuProxy2
    if (Config::PACKET_SNIFFER_ENABLED || Config::MONSTER_TRACKER_ENABLED || Config::ATTACK_MODULE_ENABLED) {
        TcpClient::Initialize(Config::TCP_HOST, Config::TCP_PORT);
        PacketHandler::Initialize();

        if (Config::MONSTER_TRACKER_ENABLED) {
            MonsterTracker::Initialize();
        }
        if (Config::ATTACK_MODULE_ENABLED) {
            AttackModule::Initialize();
        }
    }

    // Hook de debug: loga o buffer da skill antes da criptografia/envio
    SkillHook::Install();
}

static DWORD WINAPI InitThreadProc(LPVOID lpParam) {
    SafeInitialize((HMODULE)lpParam);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved) {
    switch (dwReason) {
        case DLL_PROCESS_ATTACH: {
            DisableThreadLibraryCalls(hModule);

            InitializeLogging(hModule);
            Logger::Log("SYSTEM", "=== GG Hook DLL attach ===");

            // Instala exception handlers globais (captura crashes de qualquer módulo)
            g_VehHandle = AddVectoredExceptionHandler(1, VectoredCrashHandler);
            g_PrevExceptionFilter = SetUnhandledExceptionFilter(CrashExceptionFilter);
            Logger::Log("SYSTEM", "Crash handlers instalados (VEH=%p, Unhandled=%p)", 
                g_VehHandle, g_PrevExceptionFilter);

            // Instala hooks de rede IMEDIATAMENTE (sincrono) para capturar
            // o connect do jogo ao game server. Se esperarmos uma thread,
            // o jogo ja conectou e perdemos o socket.
            if (Config::PACKET_SNIFFER_ENABLED) {
                PacketSniffer::Install();
            }

            // O resto (TCP, monster tracker, attack module) roda em thread
            // para nao travar o loader
            HANDLE hThread = CreateThread(NULL, 0, InitThreadProc, hModule, 0, NULL);
            if (!hThread) {
                Logger::Log("SYSTEM", "FALHA ao criar thread de inicializacao");
            } else {
                CloseHandle(hThread);
            }
            break;
        }

        case DLL_PROCESS_DETACH:
            if (Config::MONSTER_TRACKER_ENABLED) MonsterTracker::Shutdown();
            if (Config::ATTACK_MODULE_ENABLED) AttackModule::Shutdown();
            PacketHandler::Shutdown();
            TcpClient::Shutdown();
            PacketSniffer::Uninstall();
            // Remove exception handlers globais
            if (g_VehHandle) RemoveVectoredExceptionHandler(g_VehHandle);
            if (g_PrevExceptionFilter) SetUnhandledExceptionFilter(g_PrevExceptionFilter);
            Logger::Log("SYSTEM", "=== GG Hook DLL Detached ===");
            Logger::Shutdown();
            break;
    }
    return TRUE;
}
