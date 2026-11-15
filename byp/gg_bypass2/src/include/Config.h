#pragma once

// ============================================================================
// Config — Configurações do gg_bypass2
// ============================================================================

namespace Config {

// --- Chave mestra ---
constexpr bool BYPASS_ENABLED = true;

// --- Logging ---
// Arquivo único de log na pasta do jogo (logs de sistema + pacotes juntos).
constexpr const char *LOG_FILENAME = "gg_bypass2.log";
constexpr bool LOG_TO_FILE = true;  // true=grava logs em disco, false=só envia via TCP pro MuProxy2

// --- Versão ---
constexpr const char *VERSION = "3.1.0";
constexpr const char *BUILD = __DATE__ " " __TIME__;

// --- Captura de pacotes de rede via TCP (MuProxy2) ---
constexpr bool PACKET_SNIFFER_ENABLED = true;
constexpr const char* PACKET_SNIFFER_TCP_HOST = "127.0.0.1";
constexpr int PACKET_SNIFFER_TCP_PORT = 44413;

// --- TCP Client (conexão com MuProxy2) ---
constexpr const char* TCP_HOST = "127.0.0.1";
constexpr int TCP_PORT = 44412;

// --- Monster Tracker ---
constexpr bool MONSTER_TRACKER_ENABLED = true;
constexpr int MONSTER_SCAN_INTERVAL_MS = 500;
constexpr float MONSTER_DEFAULT_RANGE = 0.0f;
constexpr int MONSTER_DEFAULT_FILTER = 0x7;

// --- Attack Module ---
constexpr bool ATTACK_MODULE_ENABLED = true;
constexpr int ATTACK_INTERVAL_MS = 800;
constexpr int ATTACK_DEFAULT_DELAY_MS = 800;
constexpr float ATTACK_DEFAULT_RANGE = 0.0f;
constexpr int ATTACK_DEFAULT_TYPE = 0;
constexpr int ATTACK_DEFAULT_SKILL_ID = 0;

} // namespace Config
