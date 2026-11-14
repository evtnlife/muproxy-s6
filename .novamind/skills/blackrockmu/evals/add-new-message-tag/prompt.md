# Test case: add-new-message-tag

Realistic prompt a user would actually type:

> Quero adicionar uma nova mensagem no MuProxy2 pra controlar o delay do ataque em
> runtime, sem precisar parar o ataque. Preciso mandar um novo comando pro byp.dll.
> Como eu faço isso do jeito certo, sem gambiarra?

## What a correct answer needs

- Add a class with `[MessageTag("ATTACK_DELAY")]` (or similar) in `MuProxy2/Protocol/Messages.cs`.
- Implement a static `Parse(string payload)`.
- Route it in `DllCommandService.ProcessLine()` (it's an outbound MuProxy2→byp.dll message).
- Use pipe `|` as separator, never space.
- The byp.dll side must handle the new tag too (C++), but the C# side never touches crypto.
- Should NOT invent a new separator, a new port, or put the logic in the proxy.
