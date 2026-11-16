# 9. Golden rules & debugging

## Golden rules (anti-gambiarra)
1. **Crypto is byp.dll's job (C++).** MuProxy2 (C#) never decrypts or encrypts.
2. **Messages use pipe `|`**, never space. Format: `TAG|payload`.
3. **`TcpLoopbackProxy` is a blind relay** — no inspect, no decrypt, no modify.
4. **Packet List is fed by 44413 (PKT)**, not the proxy.
5. **New messages** need `[MessageTag]` + a static `Parse()` **and** a `REGISTER_MESSAGE` handler
   on the C++ side.
6. **Hardcoded addresses** (`GameAddresses.h`) are version-specific — re-validate all offsets if
   the binary changes.
7. **XorData (Camada-2) IS applied** to attack data bytes — do not skip it.
8. **Skill attacks use C3 (SimpleModulus), NOT Layer-1** — don't reuse Layer-1 for skills.
9. **Test byp.dll standalone first** (auto-attack only). MuProxy2 is an extra control layer.
10. **Camada-2 is applied exactly ONCE**, inside `PacketInjector::Inject`. Never call
    `MuCipher::XorData` yourself before injecting.
11. **Injection has a single door**: `PacketInjector::Inject(plaintext, len)` for real plaintext,
    `PacketInjector::InjectXorData(data, len)` for bytes that are already in XorData form.
    No other module may talk to the game socket directly.
12. **The socket belongs to `GameServerConnection`** — `PacketInjector` only builds bytes.
13. **Internal channels (44412/44413) use the original ws2_32 APIs** (`OriginalApi`) so they never
    pass through the hook.
14. **New module = new file in the domain folder** (`src/<domain>/<Name>.cpp`), header in
    `src/include/`, and an entry in `build.bat` `SOURCES`. No loose `.cpp` in `src/` root.

## Debugging — common failure modes
The most frequent real task is "something is broken, why?". Match the symptom, then go to the
referenced section for the underlying fact. **Before reading `gg_bypass2.log`, read §11 (log
analysis)** — it documents the exact line formats and what is intentionally absent.

| Symptom | Likely cause | Where to look |
|---------|--------------|---------------|
| Attack packet reaches the server but is rejected / "corrupted" | **Double-encryption**: XorData applied twice. `PacketInjector::Inject` already applies XorData+Layer-1; if you feed it XorData-form bytes (e.g. from `HandleSendPacket`), it encrypts again. | §4 injection paths — the two doors must NOT share the XorData step. |
| Server sees the wrong target / attack misses | Wrong index. **Both** opcodes send the **raw** entityIndex (big-endian, `OFF_INDEX` 0x07E); the `0xEFF5` XOR is not applied to any opcode today. Also the slot index ≠ entityIndex — `& 0x3FF` does NOT work. | §4 (D0/D1), §5 (slot-index note). |
| Skill attack target wrong | 0x19 is `C1 07 19 [skillH][skillL][indexH][indexL]` — **skill first**, raw index, **no direction byte**. The old "interleaved + `^0xEFF5`" description was a misread of the capture. | §4 skill attack. |
| Skill packet rejected by the server | The SM input size is still an open question: we feed 6 bytes (`[serial]+5 payload`), the client's own send RE shows 4 bytes (`data[3..6]`, serial overwrites `data[3]`). | §4 "Open question", `byp/docs/stack_c1_07_19/`. |
| Everything breaks after updating `main.exe` | All offsets in `GameAddresses.h` are **ex603-specific**. A binary change invalidates every offset. | §5, §10 — re-validate all offsets. |
| C3 packet won't decrypt (client→server) | Keys changed (re-login) or checksum fail — read the `checksum calc/expected` line and the `Live .. Cached ..` key diff in the log. `NativeDecrypt` no longer exists; own `Decrypt` is the only path. | §11 (log analysis), §3. |
| Packet List is empty | Wrong channel. The List is fed by **44413 (PKT)**, not the 44411/55902 proxy (a blind relay). Check byp.dll's PKT connection. | §2. |
| `byp.dll` never loads | The game must load `opengl32.dll` from the **game dir**; that DLL must be the GLProxy wrapper (not System32's), and `byp.dll` must sit next to it. | §1. |
| Decryption yields garbage / off-by-one | XorData is **not self-inverse in place** — decrypt must use the *encrypted* previous byte. Also Layer-1-only decrypt gives the XorData form, not the real plaintext. | §3 (Camada-2, decryption gotcha). |
| A new module compiles but its code never runs | The `.cpp` was not added to `build.bat` `SOURCES` — the file is silently ignored. | §7 (source layout rules), §12. |
