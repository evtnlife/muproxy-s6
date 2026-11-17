// game/PlayerInfo.cpp — Envia nome/level/mapa do personagem local para o
// MuProxy2 (1x por segundo) via canal de comandos.
//
// O nome e o level vêm do UserObjectStruct (o mesmo objeto que o jogo usa
// para stats). O mapa vem de main.exe + MAP_NUMBER (DWORD). Todas as leituras
// passam por SafeRead para não derrubar o jogo se o ponteiro virar lixo.
#include "sdk.h"
#include "PlayerInfo.h"

#include <cstring>
#include <cctype>

#include "GameMemory/Player.h"
#include "GameMemory/Globals.h"
#include "UserObjectStruct.h"
#include "CharacterStruct.h"
#include "GameAddresses.h"
#include "SafeRead.h"
#include "CommandChannel.h"
#include "Logger.h"

namespace PlayerInfo {

static HANDLE g_hThread = NULL;
static volatile LONG g_Running = 0;

// Remove caracteres que quebrariam o protocolo ('|' e '=') do nome.
static void SanitizeName(char* name) {
    if (!name) return;
    for (char* p = name; *p; ++p) {
        if (*p == '|' || *p == '=') *p = '_';
    }
}

static DWORD WINAPI PlayerInfoThread(LPVOID) {
    Logger::Log("PLAYERINFO", "thread iniciada");

    while (InterlockedCompareExchange(&g_Running, 1, 1) == 1) {
        // Nome — preferir o CharacterStruct (endereço DIRETO, validado por
        // ReadAsciiName). O UserObject é um ponteiro INDIRETO que no BlackRock
        // pode apontar para lixo (ver comentário em Player.cpp), então só
        // usamos como fallback.
        char name[16] = {0};
        uintptr_t ch = GameMemory::Player::Character();
        if (ch) {
            SafeRead::ReadAsciiName((const char*)(ch + CharacterStruct::OFF_NAME), name, sizeof(name));
        }
        if (name[0] == '\0') {
            uintptr_t uo = GameMemory::Player::UserObject();
            if (uo) {
                SafeRead::ReadAsciiName((const char*)(uo + UserObjectStruct::OFF_NAME), name, sizeof(name));
            }
        }
        SanitizeName(name);

        // Level (UserObjectStruct::OFF_LEVEL, word)
        WORD level = GameMemory::Player::Level();

        // Mapa (main.exe + MAP_NUMBER, dword)
        DWORD map = 0;
        uintptr_t base = GameMemory::Globals::MainBase();
        if (base) {
            map = SafeRead::ReadDword((const void*)(base + GameAddresses::MAP_NUMBER));
        }

        // ---- Diagnóstico (a cada 5s): por que o nome vem vazio? ----
        static int diagCounter = 0;
        if (++diagCounter >= 5) {
            diagCounter = 0;
            uintptr_t base = GameMemory::Globals::MainBase();
            uintptr_t ch = GameMemory::Player::Character();
            uintptr_t uo = GameMemory::Player::UserObject();
            Logger::Log("PLAYERINFO", "diag base=0x%p char=0x%p uo=0x%p", (void*)base, (void*)ch, (void*)uo);
            if (base) {
                const BYTE* p = (const BYTE*)(base + GameAddresses::CHARACTER_PTR + CharacterStruct::OFF_NAME);
                Logger::Log("PLAYERINFO", "diag charName=%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
                    SafeRead::ReadByte(p+0), SafeRead::ReadByte(p+1), SafeRead::ReadByte(p+2), SafeRead::ReadByte(p+3),
                    SafeRead::ReadByte(p+4), SafeRead::ReadByte(p+5), SafeRead::ReadByte(p+6), SafeRead::ReadByte(p+7),
                    SafeRead::ReadByte(p+8), SafeRead::ReadByte(p+9), SafeRead::ReadByte(p+10));
                uintptr_t uoPtr = SafeRead::ReadPointer((const void*)(base + GameAddresses::USER_OBJECT_PTR));
                Logger::Log("PLAYERINFO", "diag uoPtr=0x%p", (void*)uoPtr);
                if (uoPtr) {
                    const BYTE* q = (const BYTE*)(uoPtr + UserObjectStruct::OFF_NAME);
                    Logger::Log("PLAYERINFO", "diag uoName=%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
                        SafeRead::ReadByte(q+0), SafeRead::ReadByte(q+1), SafeRead::ReadByte(q+2), SafeRead::ReadByte(q+3),
                        SafeRead::ReadByte(q+4), SafeRead::ReadByte(q+5), SafeRead::ReadByte(q+6), SafeRead::ReadByte(q+7),
                        SafeRead::ReadByte(q+8), SafeRead::ReadByte(q+9), SafeRead::ReadByte(q+10));
                }
            }
        }

        CommandChannel::SendFormat("PLAYER_INFO|name=%s|level=%u|map=%u",
                                   name, (unsigned)level, (unsigned)map);

        // Loga só quando o nome muda (evita spam de 1 linha/s no log).
        static char lastName[16] = {0};
        if (strcmp(name, lastName) != 0) {
            Logger::Log("PLAYERINFO", "name='%s' level=%u map=%u", name, (unsigned)level, (unsigned)map);
            strncpy(lastName, name, sizeof(lastName) - 1);
            lastName[sizeof(lastName) - 1] = '\0';
        }

        Sleep(1000);
    }

    Logger::Log("PLAYERINFO", "thread encerrada");
    return 0;
}

void Initialize() {
    if (InterlockedCompareExchange(&g_Running, 1, 0) != 0) return;
    g_hThread = CreateThread(NULL, 0, PlayerInfoThread, NULL, 0, NULL);
    if (!g_hThread) {
        InterlockedExchange(&g_Running, 0);
        Logger::Log("PLAYERINFO", "FALHA ao criar thread");
    }
}

void Shutdown() {
    InterlockedExchange(&g_Running, 0);
    if (g_hThread) {
        WaitForSingleObject(g_hThread, 2000);
        CloseHandle(g_hThread);
        g_hThread = NULL;
    }
}

} // namespace PlayerInfo
