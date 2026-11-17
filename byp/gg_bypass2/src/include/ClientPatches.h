#pragma once
#include "sdk.h"

// ============================================================================
// ClientPatches — patches pontuais no cliente (Main.dll / main.exe).
//
// 1) INSTÂNCIA ÚNICA (Main.dll)
// -----------------------------
// O cliente tem uma rotina de "instância única" (Main.dll, por volta do RVA
// 0x13E680 — o RVA exato muda entre versões do cliente):
//
//   push 0x<endereço do título>   ; "Blackrock MU Online"
//   push 0
//   call [FindWindowA]            ; USER32!FindWindowA  <- ponto do patch
//   test eax,eax
//   jz  <continua>                ; NÃO achou -> segue (pula o bloco do mutex)
//
// Se o FindWindowA ENCONTRA a janela de outro cliente, o código entra no
// bloco do mutex "XTEAM_MAIN_10405_<ip do servidor>" (OpenMutexA). Se esse
// mutex não existir, o cliente chama ExitProcess(0) — SAÍDA SILENCIOSA, sem
// MessageBox (é por isso que o 2º cliente "fecha" sem explicar nada).
//
// O patch troca a chamada por "xor eax,eax" (eax = 0 => FindWindowA não
// achou nada) e mantém o tamanho exato dos 6 bytes do `call [IAT]`.
//
// O call-site NÃO é um RVA fixo: o launcher atualiza o Main.dll e a rotina de
// proteção muda entre versões. O patch procura o padrão "FF 15 <endereço da
// IAT>" na memória do módulo (a IAT é resolvida pelo loader, então o padrão
// é o mesmo em qualquer build).
//
// OBSERVAÇÃO: FindWindowA é chamado UMA única vez em todo o Main.dll (o
// scan do binário confirma), então o patch é cirúrgico.
//
// 2) VERIFICAÇÃO DE ENTRADA NA SALA (main.exe)
// -------------------------------------------
// No main.exe (RVA 0x14BE0 no build atual) fica a rotina que escolhe a sala e
// manda o pacote de entrada (C1 06 F4 ...). Antes do laço que monta esse
// pacote existe um gate de "aprovação" do cliente:
//
//   8B 8D 50 EF FF FF      mov ecx, [ebp-0x10B0]        ; this
//   83 B9 A4 30 00 00 00   cmp dword [ecx+0x30A4], 0    ; lista de salas?
//   75 05                  jne <aprovado>
//   E9 <rel32>             jmp <desiste>                 ; sem lista -> nao entra
//
// [this+0x30A4] e o ponteiro da lista de salas que o servidor mandou. Se ele
// for NULL, o cliente NAO envia o pacote de entrada e fica esperando na tela
// de selecao (dc). O patch troca os 14 bytes da verificacao por NOP: o
// cliente segue SEMPRE pelo caminho "aprovado", como se a lista estivesse
// disponivel.
//
// Como no patch do FindWindowA, o alvo e achado pelo PADRAO dos bytes na
// memoria (o main.exe e protegido e o launcher pode trocar o binario).
//
// ============================================================================

namespace ClientPatches {

// Aplica o patch da instancia unica no Main.dll carregado. Idempotente.
// Devolve true se o patch da instancia unica esta ativo.
bool Apply(uintptr_t mainDllBase);

// Neutraliza a verificacao de entrada na sala no main.exe carregado.
// Idempotente. Devolve true se o patch foi aplicado.
//
// Silencioso quando nao encontra o padrao (o main.exe pode ainda estar
// sendo descompactado pelo protetor) — o chamador pode tentar de novo.
bool ApplyRoomGate(uintptr_t mainExeBase);

// Neutraliza a checagem do MUTEX local do cliente ("XTEAM_MAIN_10405_<ip do
// servidor>") no Main.dll carregado. Idempotente.
//
// O bloco do mutex e alcancado pelo FindWindowA achando a janela de outro
// cliente (ja neutralizado pelo patch acima) OU pelo bit 1 do LauncherType
// desligado, que cai direto no bloco. Neste segundo caminho o 2o cliente
// morre em ExitProcess(0) — saida silenciosa, sem MessageBox.
//
// O patch e no SALTO ("jnz" para o ExitProcess), nao na chamada: OpenMutexA
// e __stdcall (a API limpa os 12 bytes dos argumentos), entao NOPar a chamada
// deixaria a pilha torta. Neutralizando o salto, nada muda na pilha.
//
// Os slots da IAT (OpenMutexA e ExitProcess) sao resolvidos em runtime pela
// tabela de importacao do modulo carregado — sem RVA fixo.
bool ApplyMutexBypass(uintptr_t mainDllBase);

} // namespace ClientPatches
