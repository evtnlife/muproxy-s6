#include "include/DllNotifyHook.h"
#include "include/Logger.h"
#include "include/Config.h"
#include "include/PsBypass.h"
#include "include/PsPacketHook.h"
#include "include/PacketSniffer.h"
#include "include/sdk.h"

typedef LONG NTSTATUS;

typedef struct _UNICODE_STRING {
    USHORT Length;
    USHORT MaximumLength;
    PWSTR Buffer;
} UNICODE_STRING, *PUNICODE_STRING;

typedef struct _LDR_DLL_LOADED_NOTIFICATION_DATA {
    ULONG Flags;
    PUNICODE_STRING FullDllName;
    PUNICODE_STRING BaseDllName;
    PVOID DllBase;
    ULONG SizeOfImage;
} LDR_DLL_LOADED_NOTIFICATION_DATA, *PLDR_DLL_LOADED_NOTIFICATION_DATA;

typedef struct _LDR_DLL_NOTIFICATION_DATA {
    union {
        LDR_DLL_LOADED_NOTIFICATION_DATA Loaded;
    } Dll;
} LDR_DLL_NOTIFICATION_DATA, *PLDR_DLL_NOTIFICATION_DATA;

typedef VOID (CALLBACK *PLDR_DLL_NOTIFICATION_FUNCTION)(
    ULONG NotificationReason,
    PLDR_DLL_NOTIFICATION_DATA NotificationData,
    PVOID Context
);

typedef NTSTATUS (NTAPI *pLdrRegisterDllNotification)(
    ULONG Flags,
    PLDR_DLL_NOTIFICATION_FUNCTION NotificationFunction,
    PVOID Context,
    PVOID* Cookie
);

typedef NTSTATUS (NTAPI *pLdrUnregisterDllNotification)(
    PVOID Cookie
);

static PVOID g_Cookie = nullptr;

static VOID CALLBACK DllNotificationCallback(
    ULONG NotificationReason,
    PLDR_DLL_NOTIFICATION_DATA NotificationData,
    PVOID /*Context*/)
{
    if (NotificationReason != 1) { // LDR_DLL_NOTIFICATION_REASON_LOADED = 1
        return;
    }

    if (!NotificationData || !NotificationData->Dll.Loaded.BaseDllName ||
        !NotificationData->Dll.Loaded.BaseDllName->Buffer) {
        return;
    }

    PWSTR name = NotificationData->Dll.Loaded.BaseDllName->Buffer;
    int len = NotificationData->Dll.Loaded.BaseDllName->Length / sizeof(WCHAR);

    // Verifica se e PS.dll (case-insensitive)
    bool isPs = false;
    if (len >= 6) {
        isPs =
            (name[0] == L'P' || name[0] == L'p') &&
            (name[1] == L'S' || name[1] == L's') &&
            (name[2] == L'.') &&
            (name[3] == L'd' || name[3] == L'D') &&
            (name[4] == L'l' || name[4] == L'L') &&
            (name[5] == L'l' || name[5] == L'L');
    }

    if (isPs) {
        Logger::Log("DLLNOTIFY", "PS.dll carregando detectado via LdrRegisterDllNotification");
        PsBypass::Neutralize();
        PsPacketHook::Install();
    }

    if (Config::PACKET_SNIFFER_ENABLED) {
        PacketSniffer::InstallHooksForAllModules();
    }
}

bool DllNotifyHook::Install() {
    HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
    if (!hNtdll) {
        Logger::Log("DLLNOTIFY", "FALHA: ntdll nao encontrado");
        return false;
    }

    pLdrRegisterDllNotification fn = (pLdrRegisterDllNotification)GetProcAddress(
        hNtdll, "LdrRegisterDllNotification");
    if (!fn) {
        Logger::Log("DLLNOTIFY", "FALHA: LdrRegisterDllNotification nao encontrado");
        return false;
    }

    NTSTATUS status = fn(0, DllNotificationCallback, nullptr, &g_Cookie);
    if (status == 0) {
        Logger::Log("DLLNOTIFY", "LdrRegisterDllNotification registrada com sucesso");
        return true;
    } else {
        Logger::Log("DLLNOTIFY", "FALHA ao registrar notificacao: 0x%08X", status);
        return false;
    }
}

void DllNotifyHook::Uninstall() {
    if (!g_Cookie) return;

    HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
    if (!hNtdll) return;

    pLdrUnregisterDllNotification fn = (pLdrUnregisterDllNotification)GetProcAddress(
        hNtdll, "LdrUnregisterDllNotification");
    if (!fn) return;

    fn(g_Cookie);
    g_Cookie = nullptr;
    Logger::Log("DLLNOTIFY", "LdrRegisterDllNotification removida");
}

