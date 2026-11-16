# 2. Communication channels

| Port  | Name        | Direction        | Format                          | Content |
|-------|-------------|------------------|---------------------------------|---------|
| 44412 | CMD         | bidirectional    | `TAG\|payload`                  | commands (ATTACK_START…), entities (ENTITY), logs (LOG) |
| 44413 | PKT         | byp.dll → MuProxy2 | `PKT\|SEND\|<hex>` / `PKT\|RECV\|<hex>` | MU packets **already decrypted** (Layer-1 removed by byp.dll) |
| 44411 | Server List | MITM relay       | binary (not inspected)          | raw client↔server-list traffic |
| 55902 | Game Server | MITM relay       | binary (not inspected)          | raw client↔game-server traffic |

- 44412 is owned by **`net/CommandChannel.cpp`** (formerly `TcpClient.cpp`); 44413 by
  **`net/PacketChannel.cpp`**. They are two independent sockets with independent reconnect
  threads — one dying does not take the other down.

## Rules
- **44412 (CMD)**: uses `MessageRegistry` with `[MessageTag("XXX")]`. Every message is
  `TAG|payload`; parsers register via reflection.
- **44413 (PKT)**: packets arrive **already decrypted** from byp.dll. MuProxy2 only
  `Convert.FromHexString()` and routes to `AddPacket()`.
- **44411/55902 (Proxy)**: `TcpLoopbackProxy` is a **blind relay** — never inspects, decrypts,
  or modifies payloads. The Packet List is fed by 44413, **not** the proxy.

## 44412 inbound command: `SEND`
- byp.dll's **CommandChannel** (`net/CommandChannel.cpp`) reads line-based commands from
  MuProxy2; `net/PacketHandler.cpp` parses the line and dispatches it through
  `MessageRegistry`. The supported command is:
  ```
  SEND [dir] <hex>
  ```
  - `dir` is optional and **ignored** (always injects C→S). Accepted prefixes:
    `Client` / `Server` / `C->S` / `S->C`.
  - `<hex>` is the packet bytes (hex pairs). byp.dll calls
    `PacketInjector::InjectXorData(packet, len)` — i.e. it applies **Layer-1 only** (assumes the
    bytes are already in XorData form). See §3 / §4 for the double-encryption trap.

## Control-socket exclusion
- The two internal channels (44412 CMD, 44413 PKT) **never pass through the ws2_32 hooks**:
  they connect/send with the original trampolines stored in `OriginalApi`
  (`core/OriginalApi.cpp`, filled by `WinsockHooks::Install()`). That is why the old
  `IsControlSocket()` guard is gone — the bypass's own traffic is simply not intercepted, and
  the `CONNECT to 127.0.0.1:44412` spam in the log is gone with it.
- Same mechanism for the injection: `GameServerConnection::SendToGameServer` sends with the
  original `send`, so injected packets are not re-logged as `SEND`.
