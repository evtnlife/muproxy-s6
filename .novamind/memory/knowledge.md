# NovaMind Consolidated Knowledge

> **Reset em 2026-11-15** — logs de sessão antigos removidos para iniciar trabalho novo.
> O conhecimento durável do projeto BlackRockMU já vive em dois lugares; este arquivo
> volta a ser apenas o log de consolidação das próximas sessões.

## Onde está o contexto durável

- **Skill `blackrockmu`** (fonte de verdade) — `F:\blackrockmu\.novamind\skills\blackrockmu\SKILL.md`
  (índice conciso) + `references/01..10-*.md` (arquitetura, canais, criptografia, pacote de
  ataque, array de objetos, protocolo de mensagens, build/deploy, config, regras/debug, referências).
  **Carregar antes de editar `byp/gg_bypass2/` ou `byp/MuProxy2/`.**
- **Memórias persistentes** — fluxo de attach do MuProxy2 (byp.dll via DllMain do opengl32.dll,
  TCP 44412 CMD / 44413 PKT), skill como fonte de verdade, e versão do cliente **ex603**
  (offsets de `GameAddresses.h` amarrados a ela — revalidar se a versão mudar).
- **Docs de referência** — `F:\blackrockmu\byp\docs\` (inclui `attack_packet_re.md`, source of truth
  do RE de ataque). Pacotes reais: `OpenMU-master/docs/Packets/`.

## Regras rápidas
- Pasta do projeto: `F:\blackrockmu\byp\` (renomeada de `psdisabled`).
- Build: `byp/gg_bypass2/build.bat` (byp.dll) e `byp/gg_bypass2/opengl/build.bat` (opengl32.dll);
  MuProxy2 via `dotnet build`.
- Usuário se comunica em português (BR).
