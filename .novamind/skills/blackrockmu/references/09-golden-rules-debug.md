# 9. Golden rules & debugging

## Golden rules (anti-gambiarra)
1. **Crypto is byp.dll's job (C++).** MuProxy2 (C#) never decrypts or encrypts.
2. **Messages use pipe `|`**, never space. Format: `TAG|payload`.
3. **`TcpLoopbackProxy` is a blind relay** — no inspect, no decrypt, no modify.
4. **Packet List is fed by 44413 (PKT)**, not the proxy.
5. **New messages** need `[MessageTag]` + a static `Parse()`.
6. **Hardcoded addresses** (`GameAddresses.h`) are version-specific — re-validate all offsets if
   the binary changes.
7. **XorData (Camada-2) IS applied** to attack data bytes — do not skip it.
8. **Skill attacks use C3 (SimpleModulus), NOT Layer-1** — don't reuse Layer-1 for skills.
9. **Test byp.dll standalone first** (auto-attack only). MuProxy2 is an extra control layer.

## Debugging — common failure modes
The most frequent real task is "something is broken, why?". Match the symptom, then go to the
referenced section for the underlying fact.

| Symptom | Likely cause | Where to look |
|---------|--------------|---------------|
| Attack packet reaches the server but is rejected / "corrupted" | **Double-encryption**: XorData applied twice. `InjectC1Attack` already does XorData+Layer-1; if you feed it XorData-form bytes (e.g. from `HandleSendPacket`), it encrypts again. | §4 injection paths — the two callers must NOT share the XorData step. |
| Server sees the wrong target / attack misses | Wrong index. Attack 0x11 uses the **raw** entityIndex (big-endian); the `0xEFF5` XOR is for **skill 0x19 only**. Also the slot index ≠ entityIndex — `& 0x3FF` does NOT work. | §4 (D0/D1), §5 (slot-index note). |
| Skill attack target wrong | For 0x19 the target is `entityIndex ^ 0xEFF5` (the value in global `0x081C03A4` / the UI "Atk ID"), NOT the raw entity. And index/skill bytes are interleaved H/L. | §4 skill attack. |
| Everything breaks after updating `main.exe` | All offsets in `GameAddresses.h` are **ex603-specific**. A binary change invalidates every offset. | §5, §10 — re-validate all offsets. |
| C3 packet won't decrypt (client→server) | **By design** — Obj1 DecKey is zeros, so `NativeDecrypt` always fails C→S. Not a bug; don't chase it. | §3 (C3 / SimpleModulus). |
| Packet List is empty | Wrong channel. The List is fed by **44413 (PKT)**, not the 44411/55902 proxy (a blind relay). Check byp.dll's PKT connection. | §2. |
| `byp.dll` never loads | The game must load `opengl32.dll` from the **game dir**; that DLL must be the GLProxy wrapper (not System32's), and `byp.dll` must sit next to it. | §1. |
| Decryption yields garbage / off-by-one | XorData is **not self-inverse in place** — decrypt must use the *encrypted* previous byte. Also Layer-1-only decrypt gives the XorData form, not the real plaintext. | §3 (Camada-2, decryption gotcha). |
