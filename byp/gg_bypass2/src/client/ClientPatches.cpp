// client/ClientPatches.cpp — Patches no cliente:
//   1) Main.dll: checagem de "instância única" (FindWindowA)
//   2) main.exe: verificação de entrada na sala (o gate [this+0x30A4])
#include "sdk.h"
#include <cstring>

#include "ClientPatches.h"
#include "Logger.h"

namespace ClientPatches {

// RVA da entrada da IAT do USER32!FindWindowA dentro do Main.dll.
// (O VALOR da IAT é resolvido pelo loader em tempo de execução — é ele que
//  o `call [IAT]` referencia.)
static const uintptr_t FINDWINDOW_IAT_RVA = 0x00208474;

// ---------------------------------------------------------------------------
// Padrão da verificação de entrada na sala (main.exe).
//
//   8B 8D 50 EF FF FF      mov ecx, [ebp-0x10B0]          ; this
//   83 B9 A4 30 00 00 00   cmp dword [ecx+0x30A4], 0      ; lista de salas?
//   75 05                  jne <aprovado>
//   E9 <rel32>             jmp <desiste>                   ; sem lista -> nao entra
//
// O "cmp" começa 6 bytes depois do inicio do padrão; a verificação inteira
// (cmp + jne + jmp) tem 14 bytes.
// ---------------------------------------------------------------------------
static const BYTE ROOM_GATE_SIG[] = {
    0x8B, 0x8D, 0x50, 0xEF, 0xFF, 0xFF,
    0x83, 0xB9, 0xA4, 0x30, 0x00, 0x00, 0x00,
    0x75, 0x05,
    0xE9
};
static const int ROOM_GATE_SIG_LEN   = (int)sizeof(ROOM_GATE_SIG);
static const int ROOM_GATE_CMP_OFF   = 6;    // inicio do "cmp" dentro do padrao
static const int ROOM_GATE_PATCH_LEN = 14;   // cmp (7) + jne (2) + jmp (5)

static bool g_Applied = false;
static bool g_RoomGateApplied = false;
static bool g_MutexBypassApplied = false;

// ---------------------------------------------------------------------------
// Percorre as seções EXECUTÁveis do módulo procurando um padrão.
// O módulo é lido da MEMÓRIA (o PE em memória), então serve tanto para o
// Main.dll quanto para o main.exe (que é descompactado pelo protetor em
// tempo de execução).
// ---------------------------------------------------------------------------
static int FindPatternsAll(uintptr_t moduleBase, const BYTE* pattern, int patternLen,
                           BYTE** out, int maxOut)
{
    int count = 0;
    if (!moduleBase || !pattern || patternLen <= 0 || maxOut <= 0) return 0;

    BYTE* dos = (BYTE*)moduleBase;
    DWORD peOff = *(DWORD*)(dos + 0x3C);
    BYTE* nt = dos + peOff;
    WORD numSections = *(WORD*)(nt + 6);
    BYTE* opt = nt + 24;
    WORD optSize = *(WORD*)(nt + 20);
    BYTE* sections = opt + optSize;

    for (WORD s = 0; s < numSections && count < maxOut; ++s) {
        BYTE* sec = sections + s * 40;
        DWORD vaddr = *(DWORD*)(sec + 12);
        DWORD vsize = *(DWORD*)(sec + 8);
        DWORD chars = *(DWORD*)(sec + 36);
        if (!(chars & 0x20000000)) continue;      // IMAGE_SCN_MEM_EXECUTE
        if (vsize == 0 || vsize > 0x40000000) continue;

        BYTE* start = (BYTE*)(moduleBase + vaddr);
        for (DWORD i = 0; i + patternLen <= vsize && count < maxOut; ++i) {
            if (memcmp(start + i, pattern, patternLen) == 0) {
                out[count++] = start + i;
            }
        }
    }
    return count;
}

static BYTE* FindPattern(uintptr_t moduleBase, const BYTE* pattern, int patternLen)
{
    BYTE* hits[1] = { NULL };
    if (FindPatternsAll(moduleBase, pattern, patternLen, hits, 1) == 1) return hits[0];
    return NULL;
}

// Tamanho da imagem do módulo (SizeOfImage), para validar os saltos.
static DWORD ImageSize(uintptr_t moduleBase)
{
    if (!moduleBase) return 0;
    BYTE* dos = (BYTE*)moduleBase;
    DWORD peOff = *(DWORD*)(dos + 0x3C);
    BYTE* nt = dos + peOff;
    BYTE* opt = nt + 24;
    return *(DWORD*)(opt + 56);
}

// Procura o call-site de FindWindowA dentro do Main.dll pelo padrão
// "FF 15 <endereço do SLOT da IAT>" (6 bytes).
//
// POR QUE BUSCAR E NÃO USAR UM RVA FIXO
// -------------------------------------
// O launcher atualiza o Main.dll sozinho, e a rotina de proteção muda entre
// versões (o RVA do call-site já foi diferente entre builds). Como a IAT é
// resolvida pelo loader, o PADRÃO do call continua o mesmo em qualquer
// versão: procuramos os bytes do `call [IAT]` com o ENDEREÇO DO SLOT
// (base + RVA da IAT — é o que o `call [IAT]` referencia; o VALOR que está
// dentro do slot é o endereço da função, que é outra coisa).
static BYTE* FindCallSite(uintptr_t mainDllBase)
{
    if (!mainDllBase) return NULL;

    uintptr_t slotAddr = mainDllBase + FINDWINDOW_IAT_RVA;

    BYTE pattern[6];
    pattern[0] = 0xFF;
    pattern[1] = 0x15;
    memcpy(pattern + 2, &slotAddr, 4);

    return FindPattern(mainDllBase, pattern, 6);
}

bool Apply(uintptr_t mainDllBase)
{
    if (g_Applied) return true;
    if (!mainDllBase) return false;   // Main.dll ainda nao carregou

    // Confere que a IAT realmente é o USER32!FindWindowA: se o layout do
    // binário mudar, o patch seria aplicado no lugar errado.
    HMODULE user32 = GetModuleHandleA("user32.dll");
    FARPROC realFindWindow = user32 ? GetProcAddress(user32, "FindWindowA") : NULL;
    void** iat = (void**)(mainDllBase + FINDWINDOW_IAT_RVA);
    if (!realFindWindow || *iat != (void*)realFindWindow) {
        Logger::Log("CLIENT", "IAT de FindWindowA nao confere (0x%p != 0x%p) — patch cancelado",
                    *iat, (void*)realFindWindow);
        return false;
    }

    BYTE* target = FindCallSite(mainDllBase);
    if (!target) {
        Logger::Log("CLIENT", "call [FindWindowA] nao encontrado no Main.dll — patch cancelado");
        return false;
    }

    DWORD oldProtect = 0;
    if (!VirtualProtect(target, 6, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        Logger::Log("CLIENT", "VirtualProtect falhou no patch de FindWindowA");
        return false;
    }

    // "call [IAT]" (6 bytes) -> "xor eax,eax" + 4 NOP (6 bytes).
    // O FindWindowA passa a devolver 0 => o cliente nao enxerga a janela de
    // outra instancia e nao entra no bloco que chama ExitProcess(0).
    BYTE patch[6];
    patch[0] = 0x31;   // xor eax,eax
    patch[1] = 0xC0;
    patch[2] = 0x90;
    patch[3] = 0x90;
    patch[4] = 0x90;
    patch[5] = 0x90;

    memcpy(target, patch, 6);
    VirtualProtect(target, 6, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), target, 6);

    g_Applied = true;
    Logger::Log("CLIENT", "checagem de instancia unica neutralizada em 0x%p (o 2o cliente nao se mata sozinho)",
                (void*)target);
    return true;
}

// ---------------------------------------------------------------------------
// Neutraliza a verificação de entrada na sala (main.exe).
//
// Sem o patch: se [this+0x30A4] (a lista de salas que o servidor mandou)
// for NULL, o cliente aborta a rotina e NÃO envia o pacote de entrada
// (C1 06 F4 ...) — ele fica parado na tela de seleção até dar dc.
//
// Com o patch: os 14 bytes da verificação viram NOP e o cliente segue sempre
// pelo caminho "aprovado".
//
// SEGURO: o laço que vem depois é limitado por [this+0x3098] (quantidade de
// salas), que só é preenchido junto com [this+0x30A4]. Se a lista estiver
// vazia, o laço não executa e o cliente sai exatamente como antes (sem
// dereference de NULL).
// ---------------------------------------------------------------------------
bool ApplyRoomGate(uintptr_t mainExeBase)
{
    if (g_RoomGateApplied) return true;
    if (!mainExeBase) return false;

    BYTE* sig = FindPattern(mainExeBase, ROOM_GATE_SIG, ROOM_GATE_SIG_LEN);
    if (!sig) return false;   // main.exe ainda descompactando — tenta de novo

    BYTE* cmpSite = sig + ROOM_GATE_CMP_OFF;   // inicio do "cmp dword [ecx+0x30A4], 0"

    // Valida o jmp de desistencia: o destino tem que estar dentro da imagem
    // do modulo (senão o padrao encontrado é outro codigo qualquer).
    DWORD imageSize = ImageSize(mainExeBase);
    LONG rel32 = 0;
    memcpy(&rel32, sig + ROOM_GATE_SIG_LEN, sizeof(rel32));
    BYTE* abortTarget = (BYTE*)(cmpSite + ROOM_GATE_PATCH_LEN + rel32);
    if (!imageSize || abortTarget < (BYTE*)mainExeBase ||
        abortTarget >= (BYTE*)(mainExeBase + imageSize)) {
        Logger::Log("CLIENT", "padrao da verificacao de sala encontrado, mas o salto nao bate (0x%p) — patch cancelado",
                    (void*)abortTarget);
        return false;
    }

    DWORD oldProtect = 0;
    if (!VirtualProtect(cmpSite, ROOM_GATE_PATCH_LEN, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        Logger::Log("CLIENT", "VirtualProtect falhou no patch da verificacao de sala");
        return false;
    }

    // A verificacao inteira (cmp + jne + jmp) vira NOP: o cliente sempre
    // segue pelo caminho "aprovado".
    memset(cmpSite, 0x90, ROOM_GATE_PATCH_LEN);
    VirtualProtect(cmpSite, ROOM_GATE_PATCH_LEN, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), cmpSite, ROOM_GATE_PATCH_LEN);

    g_RoomGateApplied = true;
    Logger::Log("CLIENT", "verificacao de entrada na sala neutralizada em 0x%p (o cliente sempre aprova a entrada)",
                (void*)cmpSite);
    return true;
}

// ---------------------------------------------------------------------------
// Neutraliza a checagem do MUTEX local ("XTEAM_MAIN_10405_<ip do servidor>").
//
// O mutex é a segunda trava de "instância única" do cliente. O bloco (que fica
// logo depois do FindWindowA) só é alcançado em dois casos:
//
//   a) o FindWindowA ACHOU a janela de outro cliente (caminho normal), ou
//   b) o bit 1 do LauncherType está DESLIGADO — nesse caso o jz do bit 1 cai
//      direto no bloco do mutex, sem passar pelo FindWindowA.
//
// O patch do FindWindowA (acima) resolve o caso (a). O caso (b) passaria
// direto, e é o que este patch cobre.
//
// Código real (Main.dll BlackrockMU, RVA 0x13E6E7 no build em disco):
//
//   13E6DE  lea  eax,[esp+18h]
//   13E6E1  push 0                 ; bInheritHandle
//   13E6E3  push 1F0001h           ; MUTEX_ALL_ACCESS
//   13E6E6  call [OpenMutexA]
//   13E6EC  test eax,eax
//   13E6EE  jnz  13E7E2            ; -> push 0 ; call [ExitProcess]  (SAI SILENCIOSO)
//   13E6F4  lea  eax,[esp+18h]
//   13E6F8  push eax               ; CreateMutexA(lpName, 0, 0)
//   ...
//
// POR QUE O PATCH É NO SALTO E NÃO NA CHAMADA
// --------------------------------------------
// OpenMutexA é __stdcall: quem limpa os 12 bytes dos argumentos é a própria
// API. Trocar a `call` por NOP deixaria 12 bytes na pilha (o epílogo da
// função, que recompõe o ESP a partir do EBP, acabaria absorvendo — mas isso
// depende do build). Patchar o SALTO é neutro: a chamada continua sendo feita
// (e continua limpando a pilha como __stdcall) e o cliente simplesmente nunca
// salta para o ExitProcess. Nenhum byte muda de lugar na pilha.
//
// Sem RVA fixo: o slot da IAT do OpenMutexA e o do ExitProcess são resolvidos
// em runtime pela tabela de importação do módulo carregado, e o call-site é
// achado pelo padrão "FF 15 <slot>".
// ---------------------------------------------------------------------------

// Converte um RVA do módulo CARREGADO em VA (o loader mapeia as seções nos
// endereços virtuais; o "raw" do PE só vale para o arquivo).
static bool RvaToVa(uintptr_t moduleBase, DWORD rva, DWORD* outVa)
{
    BYTE* dos = (BYTE*)moduleBase;
    DWORD peOff = *(DWORD*)(dos + 0x3C);
    BYTE* nt = dos + peOff;
    WORD numSections = *(WORD*)(nt + 6);
    BYTE* opt = nt + 24;
    WORD optSize = *(WORD*)(nt + 20);
    BYTE* sections = opt + optSize;

    for (WORD s = 0; s < numSections; ++s) {
        BYTE* sec = sections + s * 40;
        DWORD vaddr = *(DWORD*)(sec + 12);
        DWORD vsize = *(DWORD*)(sec + 8);
        DWORD rsize = *(DWORD*)(sec + 16);
        DWORD span = vsize > rsize ? vsize : rsize;
        if (rva >= vaddr && rva < vaddr + span) {
            *outVa = (DWORD)(moduleBase + rva);
            return true;
        }
    }
    return false;
}

// Procura o slot da IAT de uma função importada, lendo a tabela de importação
// do módulo JÁ CARREGADO. Vale para qualquer build (sem RVA fixo).
//
// ATENÇÃO: RvaToVa() já devolve o VA ABSOLUTO (base carregada + RVA) —
// somar a base de novo aqui apontaria para fora do módulo.
static bool FindIatSlot(uintptr_t moduleBase, const char* dllName, const char* funcName,
                         uintptr_t* outSlot)
{
    BYTE* dos = (BYTE*)moduleBase;
    DWORD peOff = *(DWORD*)(dos + 0x3C);
    BYTE* nt = dos + peOff;
    BYTE* opt = nt + 24;
    if (opt[0] != 0x0B) return false;             // PE32 (o cliente e 32-bit)

    DWORD imageSize = ImageSize(moduleBase);
    if (!imageSize) return false;
    BYTE* imageEnd = (BYTE*)(moduleBase + imageSize);

    DWORD impRva = *(DWORD*)(opt + 96 + 8);        // DataDirectory[1] = import
    DWORD impVa = 0;
    if (!impRva || !RvaToVa(moduleBase, impRva, &impVa)) return false;
    if ((BYTE*)impVa < (BYTE*)moduleBase || (BYTE*)impVa >= imageEnd) return false;

    BYTE* desc = (BYTE*)impVa;
    for (int d = 0; d < 128; ++d, desc += 20) {
        DWORD nameRva    = *(DWORD*)(desc + 12);
        DWORD firstThunk = *(DWORD*)(desc + 16);
        DWORD origThunk  = *(DWORD*)(desc + 0);
        if (!nameRva) break;

        DWORD nameVa = 0;
        if (!RvaToVa(moduleBase, nameRva, &nameVa)) continue;
        if ((BYTE*)nameVa < (BYTE*)moduleBase || (BYTE*)nameVa >= imageEnd) continue;
        if (_stricmp((const char*)nameVa, dllName) != 0) continue;

        DWORD thunkRva = origThunk ? origThunk : firstThunk;
        DWORD thunkVa = 0;
        if (!RvaToVa(moduleBase, thunkRva, &thunkVa)) return false;
        if ((BYTE*)thunkVa < (BYTE*)moduleBase || (BYTE*)thunkVa >= imageEnd) return false;

        for (int i = 0; i < 2048; ++i) {
            DWORD entry = *(DWORD*)(thunkVa + i * 4);
            if (!entry) break;
            if (entry & 0x80000000) continue;       // import por ordinal
            DWORD ibnVa = 0;
            if (!RvaToVa(moduleBase, entry, &ibnVa)) continue;
            if ((BYTE*)ibnVa < (BYTE*)moduleBase || (BYTE*)ibnVa + 2 >= imageEnd) continue;
            const char* fn = (const char*)(ibnVa + 2);   // Hint(2) + Name
            if (_stricmp(fn, funcName) == 0) {
                *outSlot = moduleBase + firstThunk + i * 4;
                return true;
            }
        }
        return false;                               // a DLL existe, a funcao nao
    }
    return false;
}

bool ApplyMutexBypass(uintptr_t mainDllBase)
{
    if (g_MutexBypassApplied) return true;
    if (!mainDllBase) return false;

    uintptr_t openMutexSlot = 0;
    uintptr_t exitProcSlot  = 0;
    if (!FindIatSlot(mainDllBase, "kernel32.dll", "OpenMutexA", &openMutexSlot) ||
        !FindIatSlot(mainDllBase, "kernel32.dll", "ExitProcess", &exitProcSlot)) {
        Logger::Log("CLIENT", "IAT de OpenMutexA/ExitProcess nao encontrada no Main.dll — patch do mutex cancelado");
        return false;
    }

    // "FF 15 <slot da IAT>" — o mesmo padrao do patch do FindWindowA.
    BYTE pattern[6];
    pattern[0] = 0xFF;
    pattern[1] = 0x15;
    memcpy(pattern + 2, &openMutexSlot, 4);

    BYTE* hits[8];
    int nHits = FindPatternsAll(mainDllBase, pattern, 6, hits, 8);
    if (nHits == 0) {
        Logger::Log("CLIENT", "call [OpenMutexA] nao encontrado no Main.dll — patch do mutex cancelado");
        return false;
    }

    // Alvo do salto: "push 0 ; call [ExitProcess]" (6 bytes).
    BYTE exitPat[6];
    exitPat[0] = 0x6A;
    exitPat[1] = 0x00;
    exitPat[2] = 0xFF;
    exitPat[3] = 0x15;
    memcpy(exitPat + 4, &exitProcSlot, 4);

    DWORD imageSize = ImageSize(mainDllBase);
    BYTE* imageEnd = (BYTE*)(mainDllBase + imageSize);

    for (int h = 0; h < nHits; ++h) {
        BYTE* callSite = hits[h];

        // Depois da chamada: "test eax,eax" + "jnz rel32" (salto para o ExitProcess).
        if (callSite[6] != 0x85 || callSite[7] != 0xC0 ||
            callSite[8] != 0x0F || callSite[9] != 0x85) {
            continue;
        }

        LONG rel32 = 0;
        memcpy(&rel32, callSite + 10, sizeof(rel32));
        BYTE* target = callSite + 14 + rel32;

        bool targetIsExit = false;
        for (int i = -8; i <= 8 && !targetIsExit; ++i) {
            BYTE* p = target + i;
            if (p < (BYTE*)mainDllBase || p + 6 > imageEnd) continue;
            if (memcmp(p, exitPat, 6) == 0) targetIsExit = true;
        }
        if (!targetIsExit) continue;

        DWORD oldProtect = 0;
        if (!VirtualProtect(callSite + 8, 6, PAGE_EXECUTE_READWRITE, &oldProtect)) {
            Logger::Log("CLIENT", "VirtualProtect falhou no patch do mutex");
            return false;
        }
        // O jnz (6 bytes) vira NOP: o cliente nunca salta para o ExitProcess
        // por causa do mutex local.
        memset(callSite + 8, 0x90, 6);
        VirtualProtect(callSite + 8, 6, oldProtect, &oldProtect);
        FlushInstructionCache(GetCurrentProcess(), callSite + 8, 6);

        g_MutexBypassApplied = true;
        Logger::Log("CLIENT", "mutex local neutralizado em 0x%p (o 2o cliente nao morre pelo XTEAM_MAIN_10405)",
                    (void*)(callSite + 8));
        return true;
    }

    Logger::Log("CLIENT", "nenhum call [OpenMutexA] com salto para o ExitProcess — patch do mutex cancelado");
    return false;
}

} // namespace ClientPatches
