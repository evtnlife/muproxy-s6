# 2. Communication channels

| Port  | Name        | Direction        | Format                          | Content |
|-------|-------------|------------------|---------------------------------|---------|
| 44412 | CMD         | bidirectional    | `TAG\|payload`                  | commands (ATTACK_START…), entities (ENTITY), logs (LOG) |
| 44413 | PKT         | byp.dll → MuProxy2 | `PKT\|SEND\|<hex>` / `PKT\|RECV\|<hex>` | MU packets **already decrypted** (Layer-1 removed by byp.dll) |
| 44411 | Server List | MITM relay       | binary (not inspected)          | raw client↔server-list traffic |
| 55902 | Game Server | MITM relay       | binary (not inspected)          | raw client↔game-server traffic |

## Rules
- **44412 (CMD)**: uses `MessageRegistry` with `[MessageTag("XXX")]`. Every message is
  `TAG|payload`; parsers register via reflection.
- **44413 (PKT)**: packets arrive **already decrypted** from byp.dll. MuProxy2 only
  `Convert.FromHexString()` and routes to `AddPacket()`.
- **44411/55902 (Proxy)**: `TcpLoopbackProxy` is a **blind relay** — never inspects, decrypts,
  or modifies payloads. The Packet List is fed by 44413, **not** the proxy.

## 44412 inbound command: `SEND`
- byp.dll's `TcpRecvThread` reads line-based commands from MuProxy2. The supported command is:
  ```
  SEND [dir] <hex>
  ```
  - `dir` is optional and **ignored** (always injects C→S). Accepted prefixes:
    `Client` / `Server` / `C->S` / `S->C`.
  - `<hex>` is the packet bytes (hex pairs). byp.dll calls `InjectToServer(packet, len)` — i.e. it
    applies **Layer-1 only** (assumes the bytes are already in XorData form). See §3 / §4 for the
    double-encryption trap.

## Control-socket exclusion
- byp.dll never logs or redirects its own control socket (`g_TcpSocket` → 44412).
  `IsControlSocket()` guards the ws2_32 hooks so the bypass's own traffic is not sniffed/looped.
