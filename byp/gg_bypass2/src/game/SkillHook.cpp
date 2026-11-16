// SkillHook.cpp — Hooks para capturar dados de skill attack
//
// Hook 1: 0x404D70 (SKILL_ENCRYPT_SEND)
// Hook 2: 0x4050D0 (SKILL_BUILDER_ADD_DATA)
// Hook 3: 0x998420 (SIMPLEMODULUS_ENCRYPT)
#include "SkillHook.h"
#include "InlineHook.h"
#include "Logger.h"
#include "GameMemory.h"
#include "GameAddresses.h"

#include <cstdio>
#include <cstring>

namespace SkillHook {

// ---- Hook 1: 0x404D70 (SKILL_ENCRYPT_SEND) ----
static InlineHook g_HookEncryptSend;
static volatile LONG g_HookReentrant = 0;
static void* g_TrampolineEncryptSend = nullptr;

// ---- Hook 2: 0x4050D0 (SKILL_BUILDER_ADD_DATA) ----
static InlineHook g_HookAddData;
static volatile LONG g_HookAddDataReentrant = 0;
static void* g_TrampolineAddData = nullptr;

// ---- Hook 3: 0x998420 (SIMPLEMODULUS_ENCRYPT) ----
static InlineHook g_HookSMEncrypt;
static volatile LONG g_HookSMEncryptReentrant = 0;
static void* g_TrampolineSMEncrypt = nullptr;

// Funcao C que loga os dados capturados pelo hook 0x404D70.
static void __cdecl LogEncryptSend(uintptr_t ecx_val, uintptr_t arg1, uintptr_t arg2)
{
    if (InterlockedCompareExchange(&g_HookReentrant, 1, 0) != 0) {
        return;
    }

    char hex[512];
    int hpos = 0;
    int size = 0;

    if (ecx_val) {
        __try {
            size = *(uint16_t*)ecx_val;
            if (size < 0) size = 0;
            if (size > 128) size = 128;

            for (int i = 0; i < size + 2 && hpos + 3 < (int)sizeof(hex); ++i) {
                hpos += snprintf(hex + hpos, sizeof(hex) - hpos, "%02X ",
                                 (unsigned char)((BYTE*)ecx_val)[i]);
            }
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            hpos = 0;
            hex[0] = '\0';
            size = -1;
        }
    }

    Logger::Log("SKILLHOOK",
                "0x404D70: ecx=%p arg1=%d arg2=%d size=%d data=[%s]",
                (void*)ecx_val, (int)arg1, (int)arg2, size, hex);

    // Log skill wire ID table globals (validate transformation hypothesis)
    __try {
        BYTE  flag  = *(BYTE*)(uintptr_t)GameAddresses::SKILL_TABLE_FLAG;
        DWORD index = *(DWORD*)(uintptr_t)GameAddresses::SKILL_TABLE_INDEX;
        DWORD tbase = *(DWORD*)(uintptr_t)GameAddresses::SKILL_TABLE_BASE;
        WORD  wireId = 0;
        if (flag && tbase > 0x10000) {
            wireId = *(WORD*)(tbase + (DWORD)index * 2 + 0x78);
        }
        Logger::Log("SKILLHOOK",
                    "  TABLE: flag=%d index=0x%04X base=0x%08X wireId=0x%04X",
                    (int)flag, (unsigned)index, (unsigned)tbase, (unsigned)wireId);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        Logger::Log("SKILLHOOK", "  TABLE: (access violation reading globals)");
    }

    InterlockedExchange(&g_HookReentrant, 0);
}

// Funcao C que loga os argumentos do hook 0x4050D0.
static void __cdecl LogAddData(uintptr_t ecx_val, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3, uintptr_t ret_addr)
{
    if (InterlockedCompareExchange(&g_HookAddDataReentrant, 1, 0) != 0) {
        return;
    }

    Logger::Log("SKILLHOOK",
                "0x4050D0: ecx=%p arg1=%d arg2=%d arg3=%d ret_addr=%p",
                (void*)ecx_val, (int)arg1, (int)arg2, (int)arg3, (void*)ret_addr);

    // Dump do builder object (primeiros 256 bytes em 4 linhas de 64)
    // Isso permite ver skillId, targetId e outros campos internos
    if (ecx_val && ecx_val > 0x10000) {
        __try {
            for (int row = 0; row < 4; ++row) {
                char hex[256];
                int h = 0;
                int start = row * 64;
                for (int i = start; i < start + 64 && h + 3 < (int)sizeof(hex); ++i) {
                    h += snprintf(hex + h, sizeof(hex) - h, "%02X ",
                                 (unsigned char)((BYTE*)ecx_val)[i]);
                }
                Logger::Log("SKILLHOOK", "0x4050D0 builder dump [%03X]: [%s]", start, hex);
            }
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            Logger::Log("SKILLHOOK", "0x4050D0 builder dump: (access violation)");
        }
    }

    InterlockedExchange(&g_HookAddDataReentrant, 0);
}

// Funcao C que loga o this pointer do SimpleModulus::Encrypt (0x998420)
static void __cdecl LogSMEncrypt(uintptr_t ecx_val, uintptr_t dest, uintptr_t source, uintptr_t size)
{
    if (InterlockedCompareExchange(&g_HookSMEncryptReentrant, 1, 0) != 0) {
        return;
    }

    Logger::Log("SMODHOOK", "0x998420: this=%p dest=%p source=%p size=%d",
                (void*)ecx_val, (void*)dest, (void*)source, (int)size);

    // Dump the SimpleModulus object (first 64 bytes = Modulus[4]|EncKey[4]|DecKey[4]|XORKey[4])
    if (ecx_val && ecx_val > 0x10000) {
        __try {
            DWORD* smObj = (DWORD*)ecx_val;
            Logger::Log("SMODHOOK", "SM object dump: Mod=[%04X %04X %04X %04X] Enc=[%04X %04X %04X %04X] Dec=[%04X %04X %04X %04X] Xor=[%04X %04X %04X %04X]",
                smObj[0], smObj[1], smObj[2], smObj[3],
                smObj[4], smObj[5], smObj[6], smObj[7],
                smObj[8], smObj[9], smObj[10], smObj[11],
                smObj[12], smObj[13], smObj[14], smObj[15]);
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            Logger::Log("SMODHOOK", "SM object dump: (access violation)");
        }
    }

    InterlockedExchange(&g_HookSMEncryptReentrant, 0);
}

// Naked ASM stub — abordagem limpa:
//   1. pushfd + pushad (36 bytes salvos)
//   2. Carrega ECX original e args em registradores (serao restaurados pelo popad)
//   3. Empilha args e chama LogCapture (cdecl)
//   4. popad + popfd restaura tudo
//   5. jmp para o trampoline
//
// Stack layout apos pushfd(4) + pushad(32) = 36 bytes:
//   [ESP+0]  = EFLAGS
//   [ESP+4]  = EDI      (pushad ultimo empilhado = topo)
//   [ESP+8]  = ESI
//   [ESP+12] = EBP
//   [ESP+16] = ESP(original)
//   [ESP+20] = EBX
//   [ESP+24] = EDX
//   [ESP+28] = ECX      ← ECX ORIGINAL (this pointer)
//   [ESP+32] = EAX
//   [ESP+36] = return address
//   [ESP+40] = arg1
//   [ESP+44] = arg2
static __declspec(naked) void EncryptSendHookStub()
{
    __asm {
        pushfd
        pushad

        // Carrega args em registradores
        mov   eax, [esp+28]      // ECX original (this pointer)
        mov   ecx, [esp+40]      // arg1
        mov   edx, [esp+44]      // arg2

        // Empilha args para LogEncryptSend(ecx_orig, arg1, arg2)
        push  edx                // arg2
        push  ecx                // arg1
        push  eax                // ecx_original
        call  LogEncryptSend
        add   esp, 12            // limpa 3 args (cdecl)

        // Restaura registradores
        popad
        popfd

        // Pula para o trampoline
        jmp   dword ptr [g_TrampolineEncryptSend]
    }
}

// Naked ASM stub para hook em 0x4050D0 (SKILL_BUILDER_ADD_DATA)
// 0x4050D0 prologue: 55 8B EC 51 89 4D FC = 7 bytes
//
// Stack layout apos pushfd(4) + pushad(32) = 36 bytes salvos:
//   [ESP+0]  = EFLAGS
//   [ESP+4]  = EDI
//   [ESP+8]  = ESI
//   [ESP+12] = EBP
//   [ESP+16] = ESP(original)
//   [ESP+20] = EBX
//   [ESP+24] = EDX
//   [ESP+28] = ECX      ← ECX ORIGINAL (this/slot pointer)
//   [ESP+32] = EAX
//   [ESP+36] = return address
//   [ESP+40] = arg1
//   [ESP+44] = arg2
//   [ESP+48] = arg3
static __declspec(naked) void AddDataHookStub()
{
    __asm {
        pushfd
        pushad

        // Carrega args em registradores
        mov   eax, [esp+28]      // ECX original (this/slot pointer)
        mov   ecx, [esp+40]      // arg1
        mov   edx, [esp+44]      // arg2
        mov   edi, [esp+48]      // arg3
        mov   esi, [esp+36]      // return address

        // Empilha args para LogAddData(ecx_orig, arg1, arg2, arg3, ret_addr)
        push  esi                // ret_addr
        push  edi                // arg3
        push  edx                // arg2
        push  ecx                // arg1
        push  eax                // ecx_original
        call  LogAddData
        add   esp, 20            // limpa 5 args (cdecl)

        // Restaura registradores
        popad
        popfd

        // Pula para o trampoline
        jmp   dword ptr [g_TrampolineAddData]
    }
}

// Naked ASM stub para hook em 0x998420 (SIMPLEMODULUS_ENCRYPT)
// __thiscall: ECX = this (SimpleModulus object), stack: dest, source, size
// Prologue: unknown (assume 5 bytes, adjust if needed)
//
// Stack layout apos pushfd(4) + pushad(32) = 36 bytes:
//   [ESP+28] = ECX original (this = SimpleModulus object)
//   [ESP+40] = dest
//   [ESP+44] = source
//   [ESP+48] = size
static __declspec(naked) void SMEncryptHookStub()
{
    __asm {
        pushfd
        pushad

        // Carrega args em registradores
        mov   eax, [esp+28]      // ECX original (this = SM object)
        mov   ecx, [esp+40]      // dest
        mov   edx, [esp+44]      // source
        mov   edi, [esp+48]      // size

        // Empilha args para LogSMEncrypt(ecx_orig, dest, source, size)
        push  edi                // size
        push  edx                // source
        push  ecx                // dest
        push  eax                // ecx_original (this)
        call  LogSMEncrypt
        add   esp, 16            // limpa 4 args (cdecl)

        // Restaura registradores
        popad
        popfd

        // Pula para o trampoline
        jmp   dword ptr [g_TrampolineSMEncrypt]
    }
}

static DWORD WINAPI InstallThread(LPVOID)
{
    for (int i = 0; i < 30; ++i) {
        uintptr_t base = GameMemory::Globals::MainBase();
        if (base) {
            // ---- Hook 1: SKILL_ENCRYPT_SEND (0x404D70) ----
            uintptr_t rva1 = GameAddresses::SKILL_ENCRYPT_SEND;
            void* target1 = (void*)(base + rva1);

            Logger::Log("SKILLHOOK", "Tentativa %d: hook encrypt em main.exe+0x%X (%p) [base=%p]",
                        i + 1, (unsigned)rva1, target1, (void*)base);

            if (!g_HookEncryptSend.IsInstalled()) {
                if (g_HookEncryptSend.Install(target1, (void*)EncryptSendHookStub, 7)) {
                    g_TrampolineEncryptSend = g_HookEncryptSend.GetTrampoline();
                    Logger::Log("SKILLHOOK", "Hook encrypt instalado (trampoline=%p)",
                                g_TrampolineEncryptSend);
                } else {
                    Logger::Log("SKILLHOOK", "Falha ao instalar hook encrypt (tentativa %d)", i + 1);
                }
            }

            // ---- Hook 2: SKILL_BUILDER_ADD_DATA (0x4050D0) ----
            uintptr_t rva2 = GameAddresses::SKILL_BUILDER_ADD_DATA;
            void* target2 = (void*)(base + rva2);

            Logger::Log("SKILLHOOK", "Tentativa %d: hook addData em main.exe+0x%X (%p) [base=%p]",
                        i + 1, (unsigned)rva2, target2, (void*)base);

            if (!g_HookAddData.IsInstalled()) {
                if (g_HookAddData.Install(target2, (void*)AddDataHookStub, 7)) {
                    g_TrampolineAddData = g_HookAddData.GetTrampoline();
                    Logger::Log("SKILLHOOK", "Hook addData instalado (trampoline=%p)",
                                g_TrampolineAddData);
                } else {
                    Logger::Log("SKILLHOOK", "Falha ao instalar hook addData (tentativa %d)", i + 1);
                }
            }

            // ---- Hook 3: SIMPLEMODULUS_ENCRYPT (0x998420) ----
            uintptr_t rva3 = GameAddresses::SIMPLEMODULUS_ENCRYPT;
            void* target3 = (void*)(base + rva3);

            Logger::Log("SKILLHOOK", "Tentativa %d: hook SM encrypt em main.exe+0x%X (%p) [base=%p]",
                        i + 1, (unsigned)rva3, target3, (void*)base);

            if (!g_HookSMEncrypt.IsInstalled()) {
                if (g_HookSMEncrypt.Install(target3, (void*)SMEncryptHookStub, 5)) {
                    g_TrampolineSMEncrypt = g_HookSMEncrypt.GetTrampoline();
                    Logger::Log("SKILLHOOK", "Hook SM encrypt instalado (trampoline=%p)",
                                g_TrampolineSMEncrypt);
                } else {
                    Logger::Log("SKILLHOOK", "Falha ao instalar hook SM encrypt (tentativa %d)", i + 1);
                }
            }

            // Se todos instalaram, podemos sair
            if (g_HookEncryptSend.IsInstalled() && g_HookAddData.IsInstalled() && g_HookSMEncrypt.IsInstalled()) {
                return 0;
            }
        } else {
            Logger::Log("SKILLHOOK", "Tentativa %d: main.exe ainda nao carregado", i + 1);
        }
        Sleep(1000);
    }
    Logger::Log("SKILLHOOK", "Timeout ao instalar hooks (30 tentativas)");
    return 1;
}

bool Install()
{
    if (g_HookEncryptSend.IsInstalled() && g_HookAddData.IsInstalled() && g_HookSMEncrypt.IsInstalled()) return true;

    HANDLE hThread = CreateThread(NULL, 0, InstallThread, NULL, 0, NULL);
    if (hThread) CloseHandle(hThread);
    return true;
}

void Uninstall()
{
    g_HookEncryptSend.Uninstall();
    g_HookAddData.Uninstall();
    g_HookSMEncrypt.Uninstall();
}

} // namespace SkillHook