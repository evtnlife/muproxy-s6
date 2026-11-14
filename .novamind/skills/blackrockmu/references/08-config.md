# 8. Feature flags & config (`gg_bypass2/src/include/Config.h`)

## Feature flags
| Flag | Value | Meaning |
|------|-------|---------|
| BYPASS_ENABLED | true | master on/off for the whole bypass |
| PACKET_SNIFFER_ENABLED | true | hook ws2_32 + send PKT via 44413 |
| MONSTER_TRACKER_ENABLED | true | scan ObjectArray |
| ATTACK_MODULE_ENABLED | true | auto-attack monsters |
| LOG_TO_FILE | true | write logs to disk (false = TCP to MuProxy2 only) |

## Tunables
| Constant | Value | Meaning |
|----------|-------|---------|
| PACKET_SNIFFER_TCP_PORT | 44413 | PKT channel |
| TCP_PORT | 44412 | CMD channel |
| MONSTER_SCAN_INTERVAL_MS | 500 | object-array scan period |
| MONSTER_DEFAULT_FILTER | 0x7 | monster type filter |
| ATTACK_INTERVAL_MS / ATTACK_DEFAULT_DELAY_MS | 800 | attack cadence |
| LOG_FILENAME | gg_bypass2.log | main log |
| PACKET_LOG_FILENAME | byp_packets.log | packet capture log |
| VERSION | 3.1.0 | byp.dll version |
