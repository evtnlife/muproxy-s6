#pragma once
#include "sdk.h"
#include <cstdint>
#include "UserObjectStruct.h"

// ============================================================================
// GameMemory::Player — Jogador local
//
// Três estruturas distintas:
//   Object()     — o objeto do jogador no viewport (ObjectPreview)
//   Character()  — CharacterStruct (nome, classe, skills)
//   UserObject() — UserObjectStruct (level, stats, equipment)
// ============================================================================

namespace GameMemory {
namespace Player {

// Ponteiro para o objeto do jogador local no viewport (ou null)
uintptr_t Object();

// Ponteiro para a estrutura do personagem (ou null)
// Endereço: main.exe + GameAddresses::CHARACTER_PTR (struct direta)
// Contém: nome, classe, skills. NÃO contém level/stats.
uintptr_t Character();

// Ponteiro para a estrutura de stats do personagem (ou null)
// Endereço: *(main.exe + GameAddresses::USER_OBJECT_PTR) (ponteiro indireto)
// Contém: nome, level, stats (STR/AGI/VIT/ENE)
uintptr_t UserObject();

// ---- PartyList (o jogador local é o slot 0) ----
// Life @0x10, MaxLife @0x14, LifePercent @0x18 (32 bytes por membro).
// Todas as leituras são protegidas por SEH (o ponteiro é do cliente e pode
// apontar para memória inválida → ACCESS_VIOLATION → derruba o jogo).
uintptr_t PartyMember(int slot);
int PartyLife(int slot);
int PartyMaxLife(int slot);
int PartyLifePercent(int slot);   // -1 = não pôde ler

// Define o index do jogador local (recebido do proxy via D4).
// Quando definido, Object() busca por type==1 && index==playerIndex.
void SetIndex(uint16_t index);
uint16_t Index();

// ---- Stats (UserObjectStruct) ----
// Implementados em Player.cpp com SEH: o ponteiro do ObjectCharacter e do
// cliente e pode apontar para memoria invalida -> qualquer leitura sem
// protecao derrubaria o jogo (ACCESS_VIOLATION).
WORD Level();
WORD Str();
WORD Agi();
WORD Vit();
WORD Ene();
WORD Life();
WORD MaxLife();
WORD Mana();
WORD MaxMana();
WORD Shield();
WORD MaxShield();
WORD AttackSpeed();
WORD DamageMin();
WORD DamageMax();
WORD LevelPoint();

DWORD Exp();
DWORD NextExp();
DWORD Money();
BYTE Class();
WORD MagicCount();

// Ponteiro para a lista de magias (WORD[650])
inline uintptr_t MagicList() {
    uintptr_t uo = UserObject();
    return uo ? (uo + UserObjectStruct::OFF_MAGICLIST) : 0;
}

// Ponteiro para um item do equipamento (slot 0-11)
inline uintptr_t Equipment(int slot) {
    if (slot < 0 || slot > 11) return 0;
    uintptr_t uo = UserObject();
    return uo ? (uo + UserObjectStruct::OFF_EQUIPMENT + slot * UserObjectStruct::ITEM_SIZE) : 0;
}

} // namespace Player
} // namespace GameMemory
