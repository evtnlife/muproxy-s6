#pragma once
#include "sdk.h"

// ============================================================================
// InstancePorts — Isolamento de instâncias do MuProxy2.
//
// Cada instância do MuProxy2 cuida de UM main.exe. Como o byp.dll vive
// DENTRO do main.exe, as instâncias precisam se separar.
//
// O QUE PODE E O QUE NÃO PODE MUDAR
// --------------------------------
// As portas do SERVIDOR são intocáveis: a de CS (login/server-list) é a
// 44411 e a do game server é a que o CS manda no C116. Elas não são
// deslocadas em lugar nenhum — o jogo continua conectando em 44411 e na
// porta do GS.
//
// O que muda é o ENDEREÇO LOCAL (loopback) em que o proxy escuta:
//
//   instância base 0   -> 127.0.0.1
//   instância base 100 -> 127.0.0.2
//   instância base 200 -> 127.0.0.3
//   ...
//
// O Windows escuta em todo o 127.0.0.0/8, então cada instância do MuProxy2
// consegue ter o seu próprio 127.0.0.x:44411 (e 127.0.0.x:<porta do GS>)
// sem disputar porta com a outra.
//
// E os canais INTERNOS byp <-> MuProxy2 (que são nossos, podem mudar) são
// deslocados pela base:
//
//   canal de comandos : 44412 + base
//   canal de pacotes  : 44413 + base
//
// O MuProxy2 grava a base no ambiente do processo que ele lança
// (MUPROXY_BASE) e o main.exe filho herda — o byp.dll lê no attach.
// Sem a variável (jogo aberto fora do MuProxy2) => base 0 => exatamente o
// comportamento de hoje.
//
// A pasta da sessão (MUPROXY_SESSION) é opcional: serve para o log do byp
// e para o Settings.ini ficarem por sessão.
// ============================================================================

namespace InstancePorts {

// Base da instância (0 se MUPROXY_BASE não existir ou for inválido).
int Base();

// True se a variável MUPROXY_BASE chegou neste processo (base 0 explícito
// também conta). Se for false, o byp está SEM dono — pode se cruzar com o
// MuProxy2 da base 0.
bool HasBase();

// IP de loopback desta instância ("127.0.0.1", "127.0.0.2", ...).
// É onde o proxy desta instância escuta NAS PORTAS DO SERVIDOR.
const char* LocalIp();

// Porta do canal de comandos (44412 + base).
int CommandPort();

// Porta do canal de pacotes (44413 + base).
int PacketPort();

// Pasta da sessão ("MUPROXY_SESSION") ou "" se não houver.
const char* SessionDir();

} // namespace InstancePorts
