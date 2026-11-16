#pragma once
#include <cstdint>

// ============================================================================
// GameMemory::Player::ClientVitals — HP/MP do jogador local lidos do CLIENTE
//
// O cliente MU (Main_EX603/Main/PrintPlayer.cpp) declara o estado da HUD em
// globals CONSECUTIVOS no main.exe:
//
//     DWORD ViewIndex, ViewReset, ViewValue, ViewPoint;
//     DWORD ViewCurHP, ViewMaxHP, ViewCurMP, ViewMaxMP;
//     DWORD ViewCurBP, ViewMaxBP, ViewCurSD, ViewMaxSD; ...
//
// Por que isso importa: o servidor BlackRock NÃO manda o MÁXIMO de life/mana
// em nenhum pacote (nem F3 03/E0/E1, nem C1 26 type 0xFE) — só o valor
// ATUAL. Sem o máximo o auto-potion não consegue calcular o % e nunca
// dispara.
//
// O bloco é localizado varrendo as seções graváveis do main.exe e usando
// como ASSINATURA os valores que chegaram da rede (life, mana, BP e escudo
// atuais). Quatro valores independentes casando = identificação praticamente
// única (o cliente aplica o pacote um passo atrás do sniffer, por isso a
// assinatura aceita qualquer valor recente).
// ============================================================================
namespace GameMemory {
namespace Player {

// Valores recentes vindos da rede (usados como assinatura do scan).
struct ClientVitalsHints {
    unsigned life[8];   int lifeCount = 0;
    unsigned mana[8];   int manaCount = 0;
    unsigned bp[4];     int bpCount = 0;
    unsigned shield[4]; int shieldCount = 0;
};

// Procura (uma vez) o bloco de globals da HUD.
bool ScanClientVitals(const ClientVitalsHints& hints);

// Lê o bloco já localizado. Retorna false se ainda não foi encontrado.
bool ReadClientVitals(unsigned& curLife, unsigned& maxLife,
                       unsigned& curMana, unsigned& maxMana);

// O bloco já foi localizado?
bool HasClientVitals();

// Esquece o bloco (usa quando a memória do cliente mudou).
void ResetClientVitals();

} // namespace Player
} // namespace GameMemory
