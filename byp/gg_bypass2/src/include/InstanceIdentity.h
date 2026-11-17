#pragma once
#include "sdk.h"

// ============================================================================
// InstanceIdentity — Identidade da instância no canal de comandos.
//
// QUANTO ISSO IMPORTA
// -------------------
// O MuProxy2 isola as instâncias pelo "base" (0, 100, 200...): cada instância
// escuta as portas 44411/44412/44413 + base e o byp.dll dentro do main.exe
// desloca as portas locais pelo mesmo base (MUPROXY_BASE, herdado do
// MuProxy2 no CreateProcess).
//
// A base vem do AMBIENTE. Se ela não chegar ao main.exe (jogo aberto fora do
// MuProxy2, relançamento por fora, variável apagada), o byp cai na base 0 —
// que é exatamente a base do primeiro MuProxy2. Aí o segundo jogo conversa com
// o MuProxy errado: o log do MuProxy1 recebe o tráfego do jogo 2 e os comandos
// (ataque/monstros/potion) são enviados para o jogo errado.
//
// O handshake HELLO fecha esse buraco: logo depois de conectar no canal de
// comandos, o byp se identifica (PID do main.exe + base) e o MuProxy2 só
// aceita o byp que pertence a ele. Se o byp cair na porta errada, é recusado
// com log claro em vez de ser controlado por engano.
//
// Protocolo (canal de comandos, 44412 + base):
//   byp  -> UI : "HELLO|pid=1234|base=100"
//   UI   -> byp: "HELLO_ACK|ok=1|base=100"   (aceito)
//                 "HELLO_ACK|ok=0|base=0"     (recusado: não é desta instância)
// ============================================================================

namespace InstanceIdentity {

// Instala o handler de "conectado" (manda o HELLO) e registra o handler
// do HELLO_ACK. Chamar uma vez, na inicialização do byp.
void Initialize();

// True se o MuProxy2 recusou este byp (HELLO_ACK ok=0) — ou seja, este
// main.exe NÃO pertence a esta janela do MuProxy2. Nesse estado o byp
// bloqueia as conexões do jogo: ele não pode usar o proxy de outra instância.
bool Refused();

} // namespace InstanceIdentity
