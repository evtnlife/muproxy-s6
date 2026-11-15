#include "include/PsBypass.h"
#include "include/Config.h"
#include "include/Logger.h"
#include "include/StartupHook.h"
#include <stdio.h>
#include <string.h>
#include <tlhelp32.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")

static bool IsAddressInModule(HMODULE hMod, uintptr_t address) {
  if (!hMod) return false;
  MODULEINFO mi = {};
  if (!GetModuleInformation(GetCurrentProcess(), hMod, &mi, sizeof(mi))) {
    return false;
  }
  uintptr_t base = (uintptr_t)mi.lpBaseOfDll;
  return (address >= base && address < base + mi.SizeOfImage);
}

HMODULE PsBypass::GetPsModule() {
  HMODULE hMod = GetModuleHandleA(Config::PS_DLL_NAME);
  if (!hMod) hMod = GetModuleHandleA(Config::PS_DLL_NAME_ALT);
  return hMod;
}

bool PsBypass::IsPsProtector(HMODULE hMod) {
  if (!hMod) return false;
  return GetProcAddress(hMod, "__dbk_fcall_wrapper") != nullptr ||
         GetProcAddress(hMod, "dbkFCallWrapperAddr") != nullptr;
}


bool PsBypass::RestoreGetStartupInfoA() {
  HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
  if (!hKernel32) return false;
  FARPROC pOriginal = GetProcAddress(hKernel32, "GetStartupInfoA");
  if (!pOriginal) return false;

  BYTE* ptr = (BYTE*)pOriginal;
  if (ptr[0] != 0xE9) {
    Logger::Log("PSBYPASS", "GetStartupInfoA nao esta hookado (byte=0x%02X)", ptr[0]);
    return false;
  }

  // Calcula para onde o JMP aponta (offset relativo de 32 bits)
  DWORD rel = *(DWORD*)(ptr + 1);
  BYTE* target = ptr + 5 + rel;

  // Se o JMP aponta de volta para dentro do PS.dll, eh o hook do protector.
  HMODULE hPs = GetPsModule();
  if (hPs && IsAddressInModule(hPs, (uintptr_t)target)) {
    Logger::Log("PSBYPASS", "Hook do PS.dll detectado em GetStartupInfoA (target=0x%p)", target);
  } else {
    Logger::Log("PSBYPASS", "AVISO: JMP nao aponta para PS.dll (target=0x%p)", target);
  }

  // Se o JMP aponta para o PS.dll, restaura com o prologo original Windows x86.
  // Nao usa os bytes salvos pelo StartupHook porque eles podem conter o proprio
  // JMP do protector (se o PS.dll hookou antes do bypass carregar).
  BYTE original[] = {0x8B, 0xFF, 0x55, 0x8B, 0xEC};
  if (hPs && IsAddressInModule(hPs, (uintptr_t)target)) {
    Logger::Log("PSBYPASS", "Restaurando prologo original Windows (hook do PS.dll)");
  } else {
    // Fallback: usa bytes salvos pelo StartupHook se o hook nao for do PS.dll.
    Logger::Log("PSBYPASS", "AVISO: JMP nao aponta para PS.dll; restaurando com bytes salvos");
    memcpy(original, StartupHook::g_OriginalGetStartupInfoA, 5);
  }

  DWORD oldProtect = 0;
  if (!VirtualProtect(pOriginal, 5, PAGE_EXECUTE_READWRITE, &oldProtect)) return false;
  memcpy(pOriginal, original, 5);
  VirtualProtect(pOriginal, 5, oldProtect, &oldProtect);
  FlushInstructionCache(GetCurrentProcess(), pOriginal, 5);
  Logger::Log("PSBYPASS", "GetStartupInfoA restaurado");
  return true;
}

bool PsBypass::SuspendPsThreads(HMODULE hMod) {
  if (!hMod) return false;
  DWORD pid = GetCurrentProcessId();
  HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
  if (hSnap == INVALID_HANDLE_VALUE) return false;

  THREADENTRY32 te = {};
  te.dwSize = sizeof(te);
  int suspended = 0;
  int skipped = 0;

  HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
  typedef DWORD (WINAPI *pNtQueryInformationThread)(HANDLE, DWORD, PVOID, ULONG, PULONG);
  pNtQueryInformationThread NtQIT = (pNtQueryInformationThread)GetProcAddress(hNtdll, "NtQueryInformationThread");
  if (!NtQIT) {
    Logger::Log("PSBYPASS", "AVISO: NtQueryInformationThread nao disponivel");
  }

  if (Thread32First(hSnap, &te)) {
    do {
      if (te.th32OwnerProcessID == pid && te.th32ThreadID != GetCurrentThreadId()) {
        HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME | THREAD_QUERY_INFORMATION,
                                    FALSE, te.th32ThreadID);
        if (hThread) {
          uintptr_t startAddr = 0;
          ULONG retLen = 0;
          BOOL belongsToPs = FALSE;
          if (NtQIT) {
            DWORD status = NtQIT(hThread, 9, &startAddr, sizeof(startAddr), &retLen);
            if (status != 0) {
              Logger::Log("PSBYPASS", "NtQueryInformationThread TID=%lu falhou: 0x%08X",
                          te.th32ThreadID, status);
            }
            belongsToPs = IsAddressInModule(hMod, startAddr);
          }
          if (belongsToPs) {
            DWORD prevCount = SuspendThread(hThread);
            if (prevCount != (DWORD)-1) {
              Logger::Log("PSBYPASS", "Thread %lu suspensa (start=0x%p, prev=%lu)",
                          te.th32ThreadID, (void*)startAddr, prevCount);
              suspended++;
            } else {
              Logger::Log("PSBYPASS", "FALHA ao suspender thread %lu", te.th32ThreadID);
            }
          } else {
            skipped++;
          }
          CloseHandle(hThread);
        }
      }
    } while (Thread32Next(hSnap, &te));
  }

  CloseHandle(hSnap);
  Logger::Log("PSBYPASS", "Threads do PS.dll suspensas: %d (ignoradas: %d)", suspended, skipped);
  // Sucesso mesmo se nenhuma thread do PS.dll estiver rodando (ainda).
  return true;
}

bool PsBypass::FixMainFreeLanguage() {
  const char* lang = Config::MAIN_FREE_LANGUAGE;
  size_t len = strlen(lang);
  if (len >= 4) {
    Logger::Log("PSBYPASS", "AVISO: MAIN_FREE_LANGUAGE muito longo, truncando");
    len = 3;
  }

  char* buffer = (char*)Config::MAIN_FREE_BUFFER_ADDR;
  DWORD oldProtect = 0;
  if (!VirtualProtect(buffer, 16, PAGE_READWRITE, &oldProtect)) {
    Logger::Log("PSBYPASS", "FALHA ao alterar protecao do buffer Main.free");
    return false;
  }

  memset(buffer, 0, 16);
  memcpy(buffer, lang, len);
  VirtualProtect(buffer, 16, oldProtect, &oldProtect);
  FlushInstructionCache(GetCurrentProcess(), buffer, 16);

  Logger::Log("PSBYPASS", "Buffer Main.free preenchido com idioma '%s' em 0x%p", lang, buffer);
  return true;
}

bool PsBypass::PatchMainFreeLanguageJump() {
  // Aguarda Main.dll carregar (pode ser lazy)
  HMODULE hMain = nullptr;
  for (int i = 0; i < 100 && !hMain; i++) {
    hMain = GetModuleHandleA("Main.dll");
    if (!hMain) Sleep(50);
  }
  if (!hMain) {
    Logger::Log("PSBYPASS", "Main.dll nao carregado apos timeout; patch de idioma nao aplicado");
    return false;
  }

  BYTE* addr = (BYTE*)((uintptr_t)hMain + Config::MAIN_FREE_LANGUAGE_JUMP_RVA);
  Logger::Log("PSBYPASS", "PatchMainFreeLanguageJump target=0x%p base=0x%p rva=0x%08X bytes=%02X %02X", addr, hMain, Config::MAIN_FREE_LANGUAGE_JUMP_RVA, addr[0], addr[1]);

  DWORD oldProtect = 0;
  if (!VirtualProtect(addr, 2, PAGE_EXECUTE_READWRITE, &oldProtect)) {
    Logger::Log("PSBYPASS", "FALHA ao alterar protecao do salto de idioma");
    return false;
  }

  if (addr[0] == Config::MAIN_FREE_LANGUAGE_JUMP_OPCODE_OLD) {
    addr[0] = Config::MAIN_FREE_LANGUAGE_JUMP_OPCODE_NEW;
    VirtualProtect(addr, 2, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), addr, 2);
    Logger::Log("PSBYPASS", "Main.free idioma JNZ -> JMP patch aplicado em 0x%p", addr);
    return true;
  }

  VirtualProtect(addr, 2, oldProtect, &oldProtect);
  Logger::Log("PSBYPASS", "AVISO: opcode inesperado no salto de idioma (0x%02X)", addr[0]);
  return false;
}

bool PsBypass::PatchMainFreeJump() {
  HMODULE hMain = nullptr;
  for (int i = 0; i < 100 && !hMain; i++) {
    hMain = GetModuleHandleA("Main.dll");
    if (!hMain) Sleep(50);
  }
  if (!hMain) {
    Logger::Log("PSBYPASS", "Main.dll nao carregado apos timeout; patch Main.free nao aplicado");
    return false;
  }

  BYTE* addr = (BYTE*)((uintptr_t)hMain + Config::MAIN_FREE_JUMP_RVA);
  Logger::Log("PSBYPASS", "PatchMainFreeJump target=0x%p base=0x%p rva=0x%08X bytes=%02X %02X", addr, hMain, Config::MAIN_FREE_JUMP_RVA, addr[0], addr[1]);
  DWORD oldProtect = 0;
  if (!VirtualProtect(addr, 6, PAGE_EXECUTE_READWRITE, &oldProtect)) {
    Logger::Log("PSBYPASS", "FALHA ao alterar protecao do salto Main.free");
    return false;
  }

  // A validacao usa JZ curto (74 rel8). Original: 74 1C -> EB 1C.
  if (addr[0] == 0x74) {
    addr[0] = 0xEB;
    VirtualProtect(addr, 6, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), addr, 6);
    Logger::Log("PSBYPASS", "Main.free JZ short -> JMP short patch aplicado em 0x%p", addr);
    return true;
  }

  VirtualProtect(addr, 6, oldProtect, &oldProtect);
  Logger::Log("PSBYPASS", "AVISO: opcode inesperado em Main.free JZ (0x%02X)", addr[0]);
  return false;
}

bool PsBypass::PatchPsPacketCaller() {
  // O caller esta no main.exe, nao em Main.dll. Aguarda main.exe ja ter sua imagem fixa.
  HMODULE hMainExe = GetModuleHandleA(NULL);
  if (!hMainExe) {
    Logger::Log("PSBYPASS", "main.exe nao disponivel; patch do caller PS nao aplicado");
    return false;
  }

  BYTE* addr = (BYTE*)((uintptr_t)hMainExe + Config::PS_PACKET_CALLER_RVA);
  Logger::Log("PSBYPASS", "PatchPsPacketCaller target=0x%p base=0x%p rva=0x%08X bytes=%02X %02X", addr, hMainExe, Config::PS_PACKET_CALLER_RVA, addr[0], addr[1]);

  DWORD oldProtect = 0;
  if (!VirtualProtect(addr, 2, PAGE_EXECUTE_READWRITE, &oldProtect)) {
    Logger::Log("PSBYPASS", "FALHA ao alterar protecao do caller PS");
    return false;
  }

  if (addr[0] == Config::PS_PACKET_CALLER_OPCODE_OLD) {
    addr[0] = Config::PS_PACKET_CALLER_OPCODE_NEW;
    VirtualProtect(addr, 2, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), addr, 2);
    Logger::Log("PSBYPASS", "main.exe caller PS JNZ -> JMP patch aplicado em 0x%p", addr);
    return true;
  }

  VirtualProtect(addr, 2, oldProtect, &oldProtect);
  Logger::Log("PSBYPASS", "AVISO: opcode inesperado no caller PS (0x%02X)", addr[0]);
  return false;
}

bool PsBypass::Neutralize() {
  Logger::Log("PSBYPASS", "=== Iniciando neutralizacao do PS.dll ===");

  HMODULE hPs = GetPsModule();
  if (!hPs) {
    Logger::Log("PSBYPASS", "PS.dll nao carregado ainda. Aguardando...");
    for (int i = 0; i < 30 && !hPs; i++) {
      Sleep(100);
      hPs = GetPsModule();
    }
    if (!hPs) {
      Logger::Log("PSBYPASS", "PS.dll nao foi carregado. MainPatcher aplicara patches quando Main.dll carregar.");
      return true; // MainPatcher cuida dos patches de Main.dll
    }
  }

  Logger::Log("PSBYPASS", "PS.dll encontrado em 0x%p", (void*)hPs);

  if (!IsPsProtector(hPs)) {
    Logger::Log("PSBYPASS", "AVISO: Modulo nao parece ser o protector esperado");
  }

  bool ok = true;

  if (Config::FIX_MAIN_FREE_LANGUAGE) {
    ok &= FixMainFreeLanguage();
  }

  if (Config::RESTORE_GETSTARTUPINFO) {
    ok &= RestoreGetStartupInfoA();
  }

  if (Config::SUSPEND_PS_THREADS) {
    ok &= SuspendPsThreads(hPs);
  }

  Logger::Log("PSBYPASS", "=== Neutralizacao concluida ===");
  return ok;
}
