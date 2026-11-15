#pragma once
#include <cstdint>

// ============================================================================
// ProtocolStructs — Structs de pacotes MU Online (MuEmu Protocol.h)
//
// Usados para parse de pacotes recebidos do game server.
// Todos usam #pragma pack(1) para garantir alinhamento correto.
// ============================================================================

#pragma pack(push, 1)

// ---- Headers de pacote ----
struct PMSG_HEAD {
    void set(BYTE head, BYTE size) { type = 0xC1; this->size = size; this->head = head; }
    void setE(BYTE head, BYTE size) { type = 0xC3; this->size = size; this->head = head; }
    BYTE type;
    BYTE size;
    BYTE head;
};

struct PSMSG_HEAD {
    void set(BYTE head, BYTE subh, BYTE size) { type = 0xC1; this->size = size; this->head = head; this->subh = subh; }
    void setE(BYTE head, BYTE subh, BYTE size) { type = 0xC3; this->size = size; this->head = head; this->subh = subh; }
    BYTE type;
    BYTE size;
    BYTE head;
    BYTE subh;
};

struct PWMSG_HEAD {
    void set(BYTE head, WORD size) { type = 0xC2; sizeH = HIBYTE(size); sizeL = LOBYTE(size); this->head = head; }
    void setE(BYTE head, WORD size) { type = 0xC4; sizeH = HIBYTE(size); sizeL = LOBYTE(size); this->head = head; }
    BYTE type;
    BYTE sizeH;
    BYTE sizeL;
    BYTE head;
};

// ---- Pacotes GameServer -> Client ----

// C1:11 — Damage received
struct PMSG_DAMAGE_RECV {
    PMSG_HEAD header;
    BYTE index[2];
    BYTE damage[2];
    BYTE type;
    BYTE shieldDamage[2];
};

// C1:16 — Monster die
struct PMSG_MONSTER_DIE_RECV {
    PMSG_HEAD header;
    BYTE index[2];
    BYTE experience[2];
    BYTE damage[2];
};

// C1:17 — User die
struct PMSG_USER_DIE_RECV {
    PMSG_HEAD header;
    BYTE index[2];
    BYTE skill[2];
    BYTE killer[2];
};

// C1:26 — Life update
struct PMSG_LIFE_RECV {
    PMSG_HEAD header;
    BYTE type;
    BYTE life[2];
    BYTE flag;
    BYTE shield[2];
};

// C1:27 — Mana update
struct PMSG_MANA_RECV {
    PMSG_HEAD header;
    BYTE type;
    BYTE mana[2];
    BYTE bp[2];
};

// C3:F3:03 — Character info (ALL stats)
struct PMSG_CHARACTER_INFO_RECV {
    PSMSG_HEAD header;
    BYTE X;
    BYTE Y;
    BYTE Map;
    BYTE Dir;
    BYTE Experience[8];
    BYTE NextExperience[8];
    WORD LevelUpPoint;
    WORD Strength;
    WORD Dexterity;
    WORD Vitality;
    WORD Energy;
    WORD Life;
    WORD MaxLife;
    WORD Mana;
    WORD MaxMana;
    WORD Shield;
    WORD MaxShield;
    WORD BP;
    WORD MaxBP;
    DWORD Money;
    BYTE PKLevel;
    BYTE CtlCode;
    WORD FruitAddPoint;
    WORD MaxFruitAddPoint;
    WORD Leadership;
    WORD FruitSubPoint;
    WORD MaxFruitSubPoint;
    BYTE ExtInventory;
};

// C1:F3:05 — Level up
struct PMSG_LEVEL_UP_RECV {
    PSMSG_HEAD header;
    WORD Level;
    WORD LevelUpPoint;
    WORD MaxLife;
    WORD MaxMana;
    WORD MaxShield;
    WORD MaxBP;
    WORD FruitAddPoint;
    WORD MaxFruitAddPoint;
    WORD FruitSubPoint;
    WORD MaxFruitSubPoint;
};

// C1:F3:06 — Level up point (stat add result)
struct PMSG_LEVEL_UP_POINT_RECV {
    PSMSG_HEAD header;
    BYTE result;
    WORD MaxLifeAndMana;
    WORD MaxShield;
    WORD MaxBP;
};

// C1:F3:07 — Monster damage to player
struct PMSG_MONSTER_DAMAGE_RECV {
    PSMSG_HEAD header;
    BYTE damage[2];
    BYTE shieldDamage[2];
};

// C1:F3:50 — Master info
struct PMSG_MASTER_INFO_RECV {
    PSMSG_HEAD header;
    WORD MasterLevel;
    BYTE Experience[8];
    BYTE NextExperience[8];
    WORD MasterPoint;
    WORD MaxLife;
    WORD MaxMana;
    WORD MaxShield;
    WORD MaxBP;
};

// C1:2C — Fruit result
struct PMSG_FRUIT_RESULT_RECV {
    PMSG_HEAD header;
    BYTE result;
    WORD value;
    BYTE type;
};

// C1:9C — Reward experience
struct PMSG_REWARD_EXPERIENCE_RECV {
    PMSG_HEAD header;
    BYTE index[2];
    WORD experience[2];
    BYTE damage[2];
};

// C1:A3 — Quest reward
struct PMSG_QUEST_REWARD_RECV {
    PMSG_HEAD header;
    BYTE index[2];
    BYTE QuestReward;
    BYTE QuestAmount;
};

// C1:B1:00 — Map server move
struct PMSG_MAP_SERVER_MOVE_RECV {
    PSMSG_HEAD header;
    char IpAddress[16];
    WORD ServerPort;
    WORD ServerCode;
    DWORD AuthCode1;
    DWORD AuthCode2;
    DWORD AuthCode3;
    DWORD AuthCode4;
};

// C1:F1:00 — Connect client
struct PMSG_CONNECT_CLIENT_RECV {
    PSMSG_HEAD header;
    BYTE result;
    BYTE index[2];
    BYTE ClientVersion[5];
};

// C1:F1:01 — Connect account
struct PMSG_CONNECT_ACCOUNT_RECV {
    PSMSG_HEAD header;
    BYTE result;
};

// C1:F3:00 — Character list
struct PMSG_CHARACTER_LIST_RECV {
    PSMSG_HEAD header;
    BYTE ClassCode;
    BYTE MoveCnt;
    BYTE count;
    BYTE ExtWarehouse;
};

// C3:F3:04 — Character regen
struct PMSG_CHARACTER_REGEN_RECV {
    PSMSG_HEAD header;
    BYTE X;
    BYTE Y;
    BYTE Map;
    BYTE Dir;
    WORD Life;
    WORD Mana;
    WORD Shield;
    WORD BP;
    BYTE Experience[8];
    DWORD Money;
};

#pragma pack(pop)