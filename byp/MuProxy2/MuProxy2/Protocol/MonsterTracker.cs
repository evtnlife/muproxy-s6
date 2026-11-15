using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;

namespace WinFormsMitm.Protocol
{
    /// <summary>
    /// Informações de uma entidade (monstro/NPC) rastreada.
    /// Armazena posição e tipo (Class) conforme MuEmu Protocol.h.
    /// </summary>
    public struct EntityInfo
    {
        public int X, Y;          // Posição atual (ou -1 se desconhecida)
        public int Class;         // Monster Class (type[2] do pacote 0x13, com flags removidas)
        public int TX, TY;        // Posição alvo (target position)
        public byte DirAndPkLevel; // Direção e PK Level
        public bool IsNew;        // Flag 0x80 do index (recém-spawnado)
        public bool IsTeleporting; // Flag 0x40 do index (teleportando)
        /// <summary>Raw 2-byte index as sent by the server (with flags OR'd in high byte).
        /// This is the value to use in attack/move packets sent back to the server.</summary>
        public ushort RawIndex;
        public short Level;        // Nível do monstro (do viewport 0x13 ou TargetData 0xF3-E2)
        public int MaxLife;        // Vida máxima (do viewport 0x13 ou TargetData 0xF3-E2)
        public int Life;           // Vida atual (do viewport 0x13 ou TargetData 0xF3-E2)
        public bool HasViewportData; // true se Level/MaxLife/Life vieram do viewport 0x13
        public string Name;        // Nome do monstro (do byp.dll OBJ response)

        /// <summary>
        /// Attack Target ID usado no pacote de ataque 0x11.
        /// Fórmula: AttackTargetID = EntityIndex XOR 0xEFF5
        /// Exemplo: EntityIndex 0x02A4 -> AttackIndex = 0xED51
        /// </summary>
        public ushort AttackIndex;
    }

    /// <summary>
    /// Rastreia a posição do jogador e das entidades (monstros) próximas,
    /// filtrando quais estão dentro de um raio (range) do jogador.
    /// 
    /// Baseado no protocolo MuEmu (Protocol.h / Viewport.h / Viewport.cpp):
    /// - Todos os entity indexes são 2 bytes (BYTE index[2]) com flag bits no high byte
    /// - Pacote 0x13 (viewport monster): index[2] com flags 0x80/0x40, type[2] com flags 0x80/0x70
    /// - Pacote 0x14 (viewport destroy): index[2] por entidade destruída
    /// - Pacote 0x16 (monster die): index[2] no header
    /// - Pacote 0x11 (damage): index[2] com flag 0x8000 (self-damage)
    /// - Pacote 0xD4 (move): index[2] para posição de entidades
    /// </summary>
    public sealed class MonsterTracker
    {
        private const int DefaultRange = 15; // Viewport do MU cobre ~15 tiles
        private const int MaxEntityIndex = 0x3FFF; // 14 bits, bits 15-6 são flags no MuEmu

        // Range de detecção configurável (tiles). Ligado ao txtRange da UI.
        public int Range { get; set; } = DefaultRange;

        // Posição atual do jogador (setada pelo pacote de move do próprio cliente)
        public int PlayerX { get; private set; }
        public int PlayerY { get; private set; }
        public bool HasPosition { get; private set; }

        // Entidades conhecidas: index -> EntityInfo
        private readonly Dictionary<int, EntityInfo> _entities = new(256);
        private readonly object _lock = new();

        // Index do jogador no servidor (2 bytes com flags). Setado ao receber RECV D4
        // com posição que coincide com o SEND D4, ou via pacote de login.
        private int _playerIndex;

        /// <summary>Player index no servidor (extraído do pacote de login 0xF1-0x00).
        /// 0 se ainda não foi recebido.</summary>
        public int PlayerIndex => _playerIndex;

        public event Action<int, int, int>? MonsterEnteredRange; // (index, x, y)
        public event Action<int, int, int>? MonsterMovedInRange; // (index, x, y)
        public event Action<int>? MonsterLeftRange;

        /// <summary>Disparado quando o player index é identificado (login 0xF1 ou auto-detecção via D4).</summary>
        public event Action<int>? PlayerIndexChanged;

        // Log opcional (ex: mostra no painel do MuProxy2)
        public Action<string>? Log { get; set; }

        // Eventos de UI: coordenada do jogador mudou, lista de monstros no range mudou
        public event Action<int, int>? PlayerPositionChanged;
        public event Action<IReadOnlyList<(int Index, int X, int Y, int DistSq)>>? RangeListChanged;

        // Controle opcional: processar pacotes de descoberta de entidade
        public bool EnableEntityDiscovery { get; set; } = true;

        /// <summary>
        /// Extrai o entity index real a partir de 2 bytes com flag bits.
        /// MuEmu: index[0] = SET_NUMBERHB(Index) | flags (0x80=new, 0x40=teleport)
        ///         index[1] = SET_NUMBERLB(Index)
        /// Logo: actualIndex = ((index[0] & 0x3F) << 8) | index[1]
        /// </summary>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static int ExtractEntityIndex(byte hi, byte lo)
        {
            return ((hi & 0x3F) << 8) | lo;
        }

        /// <summary>
        /// Extrai o monster Class (tipo) a partir de 2 bytes com flag bits.
        /// MuEmu: type[0] = SET_NUMBERHB(Class) | flags (0x80=castle siege ally, 0x70=creation state)
        ///         type[1] = SET_NUMBERLB(Class)
        /// Logo: actualClass = ((type[0] & 0x0F) << 8) | type[1]
        /// Nota: bits 4-6 do type[0] são creation state (0x70), bit 7 é castle siege ally (0x80).
        /// </summary>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static int ExtractMonsterClass(byte hi, byte lo)
        {
            return ((hi & 0x0F) << 8) | lo;
        }

        /// <summary>
        /// Extrai flags do index: 0x80 = recém-spawnado, 0x40 = teleportando.
        /// </summary>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static byte ExtractIndexFlags(byte hi)
        {
            return (byte)(hi & 0xC0);
        }

        /// <summary>
        /// <summary>
    /// Constante XOR para cálculo do Attack Target ID.
    /// Descoberta via engenharia reversa: AttackTargetID = EntityIndex XOR 0xEFF5
    /// Confirmado em múltiplas sessões com pacotes capturados:
    ///   0xED51 XOR 0x02A4 = 0xEFF5, 0xED40 XOR 0x02B5 = 0xEFF5,
    ///   0xED57 XOR 0x02A2 = 0xEFF5, 0xED5C XOR 0x02A9 = 0xEFF5
    /// </summary>
    public const ushort ATTACK_XOR_KEY = 0xEFF5;

    /// <summary>
    /// Computa o AttackTargetID usado no pacote de ataque 0x11.
    /// Fórmula: AttackTargetID = EntityIndex XOR 0xEFF5
    /// O EntityIndex é o valor OFF_INDEX (offset 0x07E) do ObjectManager.
    /// </summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static ushort ComputeAttackIndex(ushort entityIndex)
    {
        return (ushort)(entityIndex ^ ATTACK_XOR_KEY);
    }

    /// <summary>
    /// Processa qualquer pacote de posição conhecido (RECV ou SEND).
    /// Chamado automaticamente pelo pipeline de pacotes.
    /// </summary>
    public void OnPositionPacket(byte[] data, string dir)
        {
            if (data == null || data.Length < 4) return;
            byte header = data[0];
            int hdrSize = header switch { 0xC1 or 0xC3 => 2, 0xC2 or 0xC4 => 3, _ => 0 };
            if (hdrSize == 0 || data.Length <= hdrSize) return;

            byte opcode = data[hdrSize];
            bool isSend = dir.Contains("SEND") || dir.Contains("C->S");

            switch (opcode)
            {
                case 0xD4 when !isSend:
                    ParseRecvD4(data);
                    break;
                case 0xD4 when isSend:
                    ParseSendD4(data);
                    break;
                case 0x11:
                    // RECV 0x11 = dano (não posição). Ignorado aqui.
                    break;
                case 0x16:
                    MonsterDied(data);
                    break;
                case 0x13 when EnableEntityDiscovery:
                    ParseEntityList13(data);
                    break;
                case 0x14 when EnableEntityDiscovery:
                    ParseEntityUpdate14(data);
                    break;
                case 0x18 when !isSend:
                    // RECV 0x18 = ObjectAnimation (monstro/NPC executando animação)
                    ParseObjectAnimation18(data);
                    break;
                case 0x1F when EnableEntityDiscovery && !isSend:
                    ParseSummonedMonsters1F(data);
                    break;
                case 0x22 when EnableEntityDiscovery:
                    ParseEntityPosition22(data);
                    break;
                case 0x4E when EnableEntityDiscovery:
                    ParseEntitySpawn4E(data);
                    break;
                case 0xD8:
                    ParseEntityMoveD8(data);
                    break;
                case 0xF3:
                    ParseTargetDataF3(data);
                    break;
                case 0xF1 when !isSend:
                    ParseLoginF1(data);
                    break;
            }
        }

        /// <summary>
        /// RECV 0xD4 — Move.
        /// Formato: C1 08 D4 [indexHi] [indexLo] [x] [y] [dir]
        /// Sempre 8 bytes. O index é 2 bytes com flag bits no high byte:
        ///   bit 7 (0x80) = OBJECT_CREATE, bit 6 (0x40) = teleporting
        ///   actualIndex = (indexHi &amp; 0x3F) &lt;&lt; 8 | indexLo
        /// Não há sub-opcode — data[3] é o indexHi da entidade que se moveu.
        /// </summary>
        private void ParseRecvD4(byte[] data)
        {
            // RECV D4 é sempre C1 08 D4 [indexHi] [indexLo] [x] [y] [dir]
            if (data.Length < 8 || data[2] != 0xD4) return;

            byte idxHi = data[3], idxLo = data[4];
            int index = ExtractEntityIndex(idxHi, idxLo);
            int x = data[5];
            int y = data[6];

            // Detectar player index automaticamente:
            // Se o jogador enviou SEND D4 recentemente (HasPosition) e o RECV D4
            // tem posição próxima (±3 tiles), consideramos que este index é do jogador.
            if (HasPosition && _playerIndex == 0)
            {
                int dx = x - PlayerX, dy = y - PlayerY;
                if (dx * dx + dy * dy <= 9) // dentro de ~3 tiles
                {
                    _playerIndex = index;
                    PlayerIndexChanged?.Invoke(index);
                    Log?.Invoke($"[MON] D4 RECV detectou playerIndex={index:X4} pos=({x},{y}) player=({PlayerX},{PlayerY})");
                }
            }

            // Se o index é o do jogador, atualiza posição do jogador.
            if (_playerIndex > 0 && index == _playerIndex)
            {
                SetPlayerPosition(x, y);
                Log?.Invoke($"[MON] D4 RECV player idx={index:X4} pos=({x},{y})");
                return;
            }

            // Qualquer outra entidade é um move de monstro/NPC
            string bytesStr = string.Join("-", data.Skip(3).Take(6).Select(b => b.ToString("X2")));
            Log?.Invoke($"[MON] D4 RECV entity idx={index:X4} ({index}) idxHi={idxHi:X2} idxLo={idxLo:X2} bytes=[{bytesStr}] pos=({x},{y}) player=({PlayerX},{PlayerY})");
            
            UpdateEntity(index, x, y, Range);
        }

        /// <summary>
        /// SEND 0xD4 — posição alvo do clique/walk do jogador.
        /// Formato MuEmu (PMSG_MOVE_RECV): C1 [size] D4 [x] [y] [path...]
        /// O cliente envia posição destino, o servidor ecoa via RECV 0xD4.
        /// Nota: data[3] é a coordenada X (NÃO é sub-opcode nem index).
        /// </summary>
        private void ParseSendD4(byte[] data)
        {
            // SEND D4: C1 [size] D4 [x] [y] [path...]
            // data[3] = x, data[4] = y (posição destino do clique)
            if (data.Length < 5 || data[2] != 0xD4) return;
            int x = data[3];
            int y = data[4];
            SetPlayerPosition(x, y);
            Log?.Invoke($"[MON] D4 SEND player walk target=({x},{y})");
        }

        /// <summary>Define a posição do jogador a partir de um pacote de move RECV 0xD4 sub=0x32/0x33.</summary>
        public void UpdatePlayerPosition(byte[] data)
        {
            UpdatePlayerPositionD4(data);
        }

        /// <summary>Atualiza posição do jogador a partir de um RECV D4.
        /// Usado quando sabemos que o index é do jogador.</summary>
        private void UpdatePlayerPositionD4(byte[] data)
        {
            // C1 08 D4 [indexHi] [indexLo] [x] [y] [dir]
            if (data.Length < 8 || data[2] != 0xD4) return;
            SetPlayerPosition(data[5], data[6]);
        }

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        private void SetPlayerPosition(int x, int y)
        {
            PlayerX = x;
            PlayerY = y;
            HasPosition = true;
            PlayerPositionChanged?.Invoke(PlayerX, PlayerY);
        }

        /// <summary>
        /// Atualiza a posição de uma entidade a partir de RECV D4.
        /// Formato: C1 08 D4 [indexHi] [indexLo] [x] [y] [dir]
        /// Index é 2 bytes com flag bits: actualIndex = ((idxHi & 0x3F) << 8) | idxLo
        /// Nota: Este método agora é chamado pelo ParseRecvD4 e NÃO filtra por data[3].
        /// </summary>
        public void UpdateMonsterPosition(byte[] data, int? range = null)
        {
            if (data.Length < 8 || data[2] != 0xD4)
            {
                Log?.Invoke($"[MON] D4 descartado len={data.Length} [2]={data[2]:X2}");
                return;
            }

            // C1 08 D4 [indexHi] [indexLo] [x] [y] [dir]
            byte idxHi = data[3], idxLo = data[4];
            int index = ExtractEntityIndex(idxHi, idxLo);
            int x = data[5];
            int y = data[6];

            if (index == 0 || index > MaxEntityIndex) return;

            Log?.Invoke($"[MON] D4 RECV idx={index:X4} hi={idxHi:X2} lo={idxLo:X2} pos=({x},{y}) player=({PlayerX},{PlayerY})");
            lock (_lock) { UpdateEntity(index, x, y, range ?? Range); }
        }

        /// <summary>
        /// RECV 0x18 — ObjectAnimation.
        /// Enviado quando um objeto executa uma animação (ataque, skill, etc).
        /// Formato (C1 09 18):
        ///   C1 09 18 [objectIdHi] [objectIdLo] [direction] [animation] [targetIdHi] [targetIdLo]
        /// 
        /// ObjectId é 2 bytes big-endian (sem flags, index limpo).
        /// TargetId é 2 bytes big-endian (0 se não há alvo).
        /// Animation 0x78 = ataque normal.
        /// 
        /// Este pacote é útil para detectar quando monstros atacam o jogador
        /// e para rastrear animações de combate.
        /// </summary>
        private void ParseObjectAnimation18(byte[] data)
        {
            // C1 09 18 [objectIdHi] [objectIdLo] [direction] [animation] [targetIdHi] [targetIdLo]
            if (data.Length < 9) return;
            int hdrSize = data[0] == 0xC2 ? 3 : 2;
            byte opcode = data[hdrSize];
            if (opcode != 0x18) return;

            // ObjectId: 2 bytes big-endian (sem flags, index puro)
            byte objIdHi = data[hdrSize + 1];
            byte objIdLo = data[hdrSize + 2];
            int objectId = (objIdHi << 8) | objIdLo;

            byte direction = data[hdrSize + 3];
            byte animation = data[hdrSize + 4];

            // TargetId: 2 bytes big-endian
            byte targetIdHi = data[hdrSize + 5];
            byte targetIdLo = data[hdrSize + 6];
            int targetId = (targetIdHi << 8) | targetIdLo;

            // Animation 0x78 = ataque normal
            string animName = animation switch
            {
                0x78 => "Attack",
                _ => $"Anim0x{animation:X2}"
            };

            Log?.Invoke($"[MON] 0x18 ObjectAnimation obj={objectId:X4} dir={direction} {animName} target={targetId:X4}");

            // Registra entidades que vemos em animação mas que não estão no tracker.
            // Isso garante que monstros spawnados antes do proxy conectar ainda sejam rastreados.
            lock (_lock)
            {
            if (objectId > 0 && objectId <= MaxEntityIndex && !_entities.ContainsKey(objectId))
            {
                Log?.Invoke($"[MON] 0x18 registrando obj={objectId:X4} (não estava no tracker, sem posição)");
                RegisterEntity(objectId, -1, -1);
            }
            if (targetId > 0 && targetId <= MaxEntityIndex && !_entities.ContainsKey(targetId))
            {
                Log?.Invoke($"[MON] 0x18 registrando target={targetId:X4} (não estava no tracker, sem posição)");
                RegisterEntity(targetId, -1, -1);
            }
            }
        }

        /// <summary>
        /// RECV 0x13 — Viewport Monster List (spawn de entidades).
        /// Formato MuEmu (PMSG_VIEWPORT_SEND + PMSG_VIEWPORT_MONSTER):
        ///   C2 [sizeHi] [sizeLo] 13 [count] [registro...] [registro...]
        /// Cada registro PMSG_VIEWPORT_MONSTER (layout MuEmu):
        ///   [indexHi] [indexLo] [typeHi] [typeLo] [x] [y] [tx] [ty] [DirAndPkLevel] [count/effects...]
        ///   
        /// index[2]: high byte tem flags: 0x80=recém-spawnado, 0x40=teleportando
        ///           actualIndex = ((indexHi & 0x3F) << 8) | indexLo
        /// type[2]:  high byte tem flags: 0x80=castle siege ally, 0x70=creation state
        ///           actualClass = ((typeHi & 0x0F) << 8) | typeLo
        ///           
        /// Para GAMESERVER_UPDATE>=701, campos extras após DirAndPkLevel:
        ///   [attribute] [levelHi] [levelLo] [MaxHP 4bytes] [CurHP 4bytes]
        /// + effect list (count byte indica quantos effects seguem)
        /// 
        /// Registro base = 10 bytes (sem campos 701+):
        ///   index[2] + type[2] + x + y + tx + ty + DirAndPkLevel + count
        /// Registro 701+ = 10 + 1 + 2 + 4 + 4 = 21 bytes base + effects
        /// </summary>
        private void ParseEntityList13(byte[] data)
        {
            if (data.Length < 8) return;
            // C2 packet: data[0]=0xC2, data[1]=sizeHi, data[2]=sizeLo, data[3]=0x13, data[4]=count
            // Mas o header size pode ser C1 ou C2
            int hdrSize = data[0] == 0xC2 ? 3 : 2;
            byte opcode = data[hdrSize];
            if (opcode != 0x13) return;

            int count = data[hdrSize + 1];
            Log?.Invoke($"[MON] 0x13 EntityList count={count} len={data.Length} hdrSize={hdrSize}");

            // Cada registro tem tamanho variável. O registro base é 10 bytes:
            //   index[2] + type[2] + x + y + tx + ty + DirAndPkLevel + count
            // O campo "count" no final do registro indica quantos effects seguem,
            // e cada effect pode ter tamanho variável.
            //
            // Estratégia: ler os campos fixos (10 bytes) e depois pular os effects
            // usando o campo "count" como aproximação. Cada effect tem pelo menos 1 byte,
            // mas o tamanho real depende do tipo de effect.
            //
            // Para ser robusto, vamos ler o tamanho total do pacote e iterar
            // pelos registros usando offsets conhecidos.

            int i = hdrSize + 2; // começa após count
            int recordsParsed = 0;

            while (i + 9 <= data.Length && recordsParsed < count)
            {
                // Lê index[2] com flag masking
                byte indexHi = data[i];
                byte indexLo = data[i + 1];
                int index = ExtractEntityIndex(indexHi, indexLo);
                byte indexFlags = ExtractIndexFlags(indexHi);

                // Lê type[2] com flag masking (monster Class)
                byte typeHi = data[i + 2];
                byte typeLo = data[i + 3];
                int monsterClass = ExtractMonsterClass(typeHi, typeLo);

                // Posição: x, y, tx, ty (1 byte cada)
                int x = data[i + 4];
                int y = data[i + 5];
                int tx = data[i + 6];
                int ty = data[i + 7];
                byte dirAndPkLevel = data[i + 8];
                
                // Proteção contra IndexOutOfRangeException
                if (i + 9 >= data.Length)
                {
                    Log?.Invoke($"[MON] 0x13 buffer overflow i={i} len={data.Length}");
                    break;
                }
                byte effectCount = data[i + 9];

                // PMSG_VIEWPORT_MONSTER (GAMESERVER_UPDATE>=701):
                //   index[2] + type[2] + x + y + tx + ty + DirAndPkLevel + effectCount
                //   + attribute(1) + level(2) + MaxHP(4) + CurHP(4)  [se effectCount > 0]
                //   + effects (tamanho variável)
                int extraSize = 0;
                short vpLevel = 0;
                int vpMaxHP = 0, vpCurHP = 0;
                if (i + 10 + 11 <= data.Length && effectCount > 0)
                {
                    extraSize = 11;
                    // attribute: data[i + 10] (1 byte)
                    vpLevel = BitConverter.ToInt16(data, i + 11);        // level[2] little-endian
                    vpMaxHP = BitConverter.ToInt32(data, i + 13);         // MaxHP[4] little-endian
                    vpCurHP = BitConverter.ToInt32(data, i + 17);          // CurHP[4] little-endian
                }
                
                // Tamanho base: 10 bytes + extras + effects
                int recordSize = 10 + extraSize;
                
                // Pular effects (cada effect tem pelo menos 1 byte)
                if (effectCount > 0)
                {
                    // Efeitos: na prática cada effect tem 2-5 bytes
                    // Vamos estimar 2 bytes por effect para ser conservador
                    int effectsSize = effectCount * 2;
                    
                    // Garantir que não ultrapassa o buffer
                    if (i + recordSize + effectsSize > data.Length)
                    {
                        effectsSize = data.Length - (i + recordSize);
                    }
                    recordSize += effectsSize;
                }

                // Garantir que o recordSize não ultrapassa o buffer
                if (i + recordSize > data.Length)
                {
                    recordSize = data.Length - i;
                    if (recordSize < 10) break;
                }

                if (index != 0 && index <= MaxEntityIndex)
                {
                    ushort rawIndex = (ushort)((indexHi << 8) | indexLo);
                    Log?.Invoke($"[MON] 0x13 entity idx={index:X4} raw={rawIndex:X4} hi={indexHi:X2} lo={indexLo:X2} class={monsterClass} typeHi={typeHi:X2} typeLo={typeLo:X2} pos=({x},{y}) tx=({tx},{ty}) flags=0x{indexFlags:X2} dirPk=0x{dirAndPkLevel:X2} effects={effectCount} lvl={vpLevel} hp={vpCurHP}/{vpMaxHP}");
                    RegisterEntity(index, x, y, monsterClass, tx, ty, dirAndPkLevel, indexFlags, rawIndex, idxLo: indexLo, classLo: typeLo, level: vpLevel, maxLife: vpMaxHP, curLife: vpCurHP);
                }

                i += recordSize;
                recordsParsed++;
            }
        }

        /// <summary>
        /// RECV 0x14 — Viewport Destroy (lista de entidades destruídas).
        /// Formato MuEmu: C1 [size] 14 [count] [indexHi] [indexLo] ...
        /// Cada index é 2 bytes (PMSG_VIEWPORT_DESTROY = BYTE index[2]).
        /// O index pode conter flags nos bits altos, então usamos ExtractEntityIndex.
        /// </summary>
        private void ParseEntityUpdate14(byte[] data)
        {
            if (data.Length < 5) return;
            int hdrSize = data[0] == 0xC2 ? 3 : 2;
            byte opcode = data[hdrSize];
            if (opcode != 0x14) return;

            int count = data[hdrSize + 1];
            Log?.Invoke($"[MON] 0x14 ViewportDestroy count={count} len={data.Length}");

            int pos = hdrSize + 2;
            for (int i = 0; i < count && pos + 1 < data.Length; i++, pos += 2)
            {
                int index = ExtractEntityIndex(data[pos], data[pos + 1]);
                if (index == 0 || index > MaxEntityIndex) continue;
                Log?.Invoke($"[MON] 0x14 destroy idx={index:X4} hi={data[pos]:X2} lo={data[pos+1]:X2}");
                RemoveMonster(index);
            }
        }

        /// <summary>
        /// RECV 0x22 — EntityPosition.
        /// Formato: C1 [size] 22 [indexHi] [indexLo] [x] [y] [dir?]
        /// Index é 2 bytes com flag bits.
        /// </summary>
        private void ParseEntityPosition22(byte[] data)
        {
            if (data.Length < 8) return;
            int hdrSize = data[0] == 0xC2 ? 3 : 2;
            byte opcode = data[hdrSize];
            if (opcode != 0x22) return;

            byte idxHi = data[hdrSize + 1], idxLo = data[hdrSize + 2];
            int index = ExtractEntityIndex(idxHi, idxLo);
            ushort rawIndex = (ushort)((idxHi << 8) | idxLo);
            if (index == 0 || index > MaxEntityIndex) return;
            int x = data[hdrSize + 3];
            int y = data[hdrSize + 4];
            Log?.Invoke($"[MON] 0x22 entity idx={index:X4} raw={rawIndex:X4} hi={idxHi:X2} lo={idxLo:X2} pos=({x},{y})");
            RegisterEntity(index, x, y, rawIndex: rawIndex);
        }

        /// <summary>
        /// RECV 0x4E — EntitySpawn (monstro/entidade aparece no mapa).
        /// Layout mais comum: C2 [len2] [len1] 4E [indexHi] [indexLo] [type] [x] [y] ...
        /// </summary>
        private void ParseEntitySpawn4E(byte[] data)
        {
            if (data.Length < 10) return;
            int hdrSize = data[0] == 0xC2 ? 3 : 2;
            byte opcode = data[hdrSize];
            if (opcode != 0x4E) return;

            byte idxHi = data[hdrSize + 1], idxLo = data[hdrSize + 2];
            int index = ExtractEntityIndex(idxHi, idxLo);
            ushort rawIndex = (ushort)((idxHi << 8) | idxLo);
            if (index == 0 || index > MaxEntityIndex) return;

            if (data.Length > hdrSize + 5)
            {
                int x = data[hdrSize + 4];
                int y = data[hdrSize + 5];
                Log?.Invoke($"[MON] 0x4E spawn idx={index:X4} raw={rawIndex:X4} hi={idxHi:X2} lo={idxLo:X2} pos=({x},{y})");
                RegisterEntity(index, x, y, rawIndex: rawIndex);
            }
            else
            {
                RegisterEntity(index, -1, -1, rawIndex: rawIndex);
            }
        }

        /// <summary>
        /// RECV 0x1F — AddSummonedMonstersToScope (OpenMU: C2 1F).
        /// Enviado pelo servidor quando monstros invocados (summoned) entram no scope do jogador.
        /// Layout (C2 packet):
        ///   C2 [sizeHi] [sizeLo] 1F [count] [registro...] [registro...]
        /// Cada registro SummonedMonsterData:
        ///   [indexHi] [indexLo] [typeHi] [typeLo] [x] [y] [tx] [ty] [rot&4bits] [OwnerName 10bytes] [effectCount] [effects...]
        /// Index e Type usam o mesmo esquema de flag bits do pacote 0x13.
        /// </summary>
        private void ParseSummonedMonsters1F(byte[] data)
        {
            // C2 packet: data[0]=0xC2, data[1]=sizeHi, data[2]=sizeLo, data[3]=0x1F, data[4]=count
            if (data.Length < 5) return;
            int hdrSize = data[0] == 0xC2 ? 3 : 2;
            byte opcode = data[hdrSize];
            if (opcode != 0x1F) return;

            int count = data[hdrSize + 1];
            Log?.Invoke($"[MON] 0x1F SummonedMonsters count={count} len={data.Length}");

            // Cada registro SummonedMonsterData tem tamanho variável:
            //   Base: 2(index) + 2(type) + 1(x) + 1(y) + 1(tx) + 1(ty) + 1(rot) + 10(name) + 1(effectCount) = 20 bytes
            //   + effects: effectCount bytes (cada effect = 1 byte)
            int pos = hdrSize + 2; // começa após count
            int recordsParsed = 0;

            while (pos + 20 <= data.Length && recordsParsed < count)
            {
                // Lê index[2] com flag masking
                byte indexHi = data[pos];
                byte indexLo = data[pos + 1];
                int index = ExtractEntityIndex(indexHi, indexLo);
                byte indexFlags = ExtractIndexFlags(indexHi);
                ushort rawIndex = (ushort)((indexHi << 8) | indexLo);

                // Lê type[2] com flag masking (monster Class)
                byte typeHi = data[pos + 2];
                byte typeLo = data[pos + 3];
                int monsterClass = ExtractMonsterClass(typeHi, typeLo);

                // Posição: x, y, tx, ty
                int x = data[pos + 4];
                int y = data[pos + 5];
                int tx = data[pos + 6];
                int ty = data[pos + 7];

                // Rotation (4 bits) no byte data[pos+8]
                byte dirAndPkLevel = data[pos + 8];

                // OwnerCharacterName: 10 bytes (pos+9 a pos+18)
                // string ownerName = Encoding.ASCII.GetString(data, pos + 9, 10).TrimEnd('\0');

                // EffectCount no byte pos+19
                byte effectCount = (data.Length > pos + 19) ? data[pos + 19] : (byte)0;

                // Tamanho total do registro: 20 bytes base + effectCount bytes de effects
                int recordSize = 20 + effectCount;

                if (index != 0 && index <= MaxEntityIndex)
                {
                    Log?.Invoke($"[MON] 0x1F summoned idx={index:X4} raw={rawIndex:X4} class={monsterClass} pos=({x},{y}) flags=0x{indexFlags:X2} effects={effectCount}");
                    RegisterEntity(index, x, y, monsterClass, tx, ty, dirAndPkLevel, indexFlags, rawIndex, idxLo: indexLo, classLo: typeLo);
                }

                pos += recordSize;
                recordsParsed++;
            }
        }

        /// <summary>
        /// RECV 0xD8 — Elemental Damage / EntityMove.
        /// Formato MuEmu (PMSG_ELEMENTAL_DAMAGE_SEND): C1 [size] D8 [indexHi] [indexLo] [attribute] [damage DWORD]
        /// Também pode ser movimento de entidade em alguns servidores.
        /// </summary>
        private void ParseEntityMoveD8(byte[] data)
        {
            if (data.Length < 8) return;
            int hdrSize = data[0] == 0xC2 ? 3 : 2;
            byte opcode = data[hdrSize];
            if (opcode != 0xD8) return;

            int index = ExtractEntityIndex(data[hdrSize + 1], data[hdrSize + 2]);
            if (index == 0 || index > MaxEntityIndex) return;

            // D8 é elemental damage, não movimento. Não atualiza posição.
            // Se houver dados de posição, podemos atualizar, mas geralmente não há.
            Log?.Invoke($"[MON] 0xD8 elemental damage idx={index:X4}");
        }

        /// <summary>Registra/atualiza entidade com posição e tipo (monster Class).</summary>
        private void RegisterEntity(int index, int x, int y, int monsterClass, int tx = -1, int ty = -1, byte dirAndPkLevel = 0, byte indexFlags = 0, ushort rawIndex = 0, byte idxLo = 0, byte classLo = 0, short level = 0, int maxLife = 0, int curLife = 0)
        {
            lock (_lock)
            {
            bool existed = _entities.TryGetValue(index, out var prev);
            // Compute AttackTargetID: EntityIndex XOR 0xEFF5
            ushort attackIndex = ComputeAttackIndex((ushort)index);
            if (attackIndex == ATTACK_XOR_KEY && existed)
                attackIndex = prev.AttackIndex; // preserve existing attack index if entity is 0
            // Se veio com dados de viewport (level/maxLife/curLife), usa-os;
            // senão preserva dados anteriores (podem ter vindo de F3-E2)
            bool hasViewportData = level > 0 || maxLife > 0 || curLife > 0;
            var info = new EntityInfo
            {
                X = x,
                Y = y,
                Class = monsterClass,
                TX = tx >= 0 ? tx : (existed ? prev.TX : -1),
                TY = ty >= 0 ? ty : (existed ? prev.TY : -1),
                DirAndPkLevel = dirAndPkLevel != 0 ? dirAndPkLevel : (existed ? prev.DirAndPkLevel : (byte)0),
                IsNew = (indexFlags & 0x80) != 0,
                IsTeleporting = (indexFlags & 0x40) != 0,
                RawIndex = rawIndex != 0 ? rawIndex : (existed ? prev.RawIndex : (ushort)index),
                AttackIndex = attackIndex,
                Level = hasViewportData ? level : (existed ? prev.Level : (short)0),
                MaxLife = hasViewportData ? maxLife : (existed ? prev.MaxLife : 0),
                Life = hasViewportData ? curLife : (existed ? prev.Life : 0),
                HasViewportData = hasViewportData || (existed && prev.HasViewportData),
            };
            _entities[index] = info;

            Log?.Invoke($"[MON] Register idx={index:X4} raw={info.RawIndex:X4} class={monsterClass} attackIdx={attackIndex:X4} idxLo={idxLo:X2} classLo={classLo:X2} lvl={info.Level} hp={info.Life}/{info.MaxLife} pos=({x},{y}) player=({PlayerX},{PlayerY}) flags=0x{indexFlags:X2} isNew={info.IsNew} isTele={info.IsTeleporting}");

            if (x >= 0 && y >= 0)
            {
                UpdateEntity(index, x, y, Range);
            }
            else
            {
                RangeListChanged?.Invoke(EntitiesInRangeDetailedLocked(Range));
            }
            } // lock
        }

        /// <summary>Registra/atualiza entidade com posição apenas (sem tipo).</summary>
        private void RegisterEntity(int index, int x, int y, int? range = null, ushort rawIndex = 0)
        {
            lock (_lock)
            {
            bool existed = _entities.TryGetValue(index, out var prev);
            var info = new EntityInfo
            {
                X = x,
                Y = y,
                Class = existed ? prev.Class : 0,
                TX = existed ? prev.TX : -1,
                TY = existed ? prev.TY : -1,
                DirAndPkLevel = existed ? prev.DirAndPkLevel : (byte)0,
                IsNew = false,
                IsTeleporting = false,
                RawIndex = rawIndex != 0 ? rawIndex : (existed ? prev.RawIndex : (ushort)index),
                AttackIndex = existed ? prev.AttackIndex : (ushort)0,
            };
            _entities[index] = info;

            Log?.Invoke($"[MON] Register idx={index:X4} raw={info.RawIndex:X4} attackIdx={info.AttackIndex:X4} pos=({x},{y}) player=({PlayerX},{PlayerY})");
            if (x >= 0 && y >= 0)
            {
                UpdateEntity(index, x, y, range ?? Range);
            }
            else
            {
                RangeListChanged?.Invoke(EntitiesInRangeDetailedLocked(range ?? Range));
            }
            } // lock
        }

        private void UpdateEntity(int index, int x, int y, int range)
        {
            // NOTA: caller (RegisterEntity) já detém _lock
            var entities = _entities;
            bool existed = entities.TryGetValue(index, out var prev);

            // Se ainda nao tem posicao do jogador, guarda mas nao avalia range
            if (!HasPosition)
            {
                var entry = prev;
                entry.X = x;
                entry.Y = y;
                if (!existed) entry.RawIndex = (ushort)index;
                entities[index] = entry;
                return;
            }

            int rangeSq = range * range;
            int px = PlayerX, py = PlayerY;
            int dx = x - px, dy = y - py;
            bool inRange = (dx * dx + dy * dy) <= rangeSq;

            if (!inRange)
            {
                // Fora do range -> remove da lista e dispara evento
                if (existed && entities.Remove(index))
                {
                    Log?.Invoke($"[MON] idx={index:X4} saiu do range -> removido (pos era {prev.X},{prev.Y})");
                    MonsterLeftRange?.Invoke(index);
                    RangeListChanged?.Invoke(EntitiesInRangeDetailedLocked(range));
                }
                return;
            }

            var info = prev;
            info.X = x;
            info.Y = y;
            if (!existed) info.RawIndex = (ushort)index;
            entities[index] = info;

            if (!existed || prev.X < 0)
            {
                Log?.Invoke($"[MON] idx={index:X4} entrou no range em ({x},{y}) class={info.Class} raw={info.RawIndex:X4}");
                MonsterEnteredRange?.Invoke(index, x, y);
            }
            else
            {
                bool wasInRange = (prev.X - px) * (prev.X - px)
                                + (prev.Y - py) * (prev.Y - py) <= rangeSq;
                if (!wasInRange)
                {
                    Log?.Invoke($"[MON] idx={index:X4} entrou no range em ({x},{y}) class={info.Class} raw={info.RawIndex:X4}");
                    MonsterEnteredRange?.Invoke(index, x, y);
                }
                else
                    MonsterMovedInRange?.Invoke(index, x, y);
            }
            RangeListChanged?.Invoke(EntitiesInRangeDetailedLocked(range));
        }

        /// <summary>Remove um monstro da lista (ex: quando ele morre).</summary>
        public void RemoveMonster(int index)
        {
            lock (_lock)
            {
            if (_entities.Remove(index))
            {
                Log?.Invoke($"[MON] idx={index:X4} removido (morreu/saiu)");
                MonsterLeftRange?.Invoke(index);
                RangeListChanged?.Invoke(EntitiesInRangeDetailedLocked(Range));
            }
            }
        }

        /// <summary>Verifica se um monstro está na lista.</summary>
        public bool ContainsMonster(int index) { lock (_lock) return _entities.ContainsKey(index); }

        /// <summary>Remove um monstro a partir do pacote de morte (0x16 = MonsterDie).</summary>
        /// <remarks>
        /// Formato MuEmu (PMSG_MONSTER_DIE_SEND): C1 [size] 16 [indexHi] [indexLo] [experienceHi] [experienceLo] [damageHi] [damageLo]
        /// O index é 2 bytes com possíveis flag bits: actualIndex = ExtractEntityIndex(hi, lo)
        /// </remarks>
        public void MonsterDied(byte[] data)
        {
            // C1 [size] 16 [indexHi] [indexLo] ... = no mínimo 5 bytes
            if (data.Length < 5) return;

            int hdrSize = data[0] == 0xC2 ? 3 : 2;
            byte opcode = data[hdrSize];
            if (opcode != 0x16) return;

            // Index é 2 bytes (conforme MuEmu PMSG_MONSTER_DIE_SEND)
            if (data.Length >= hdrSize + 3)
            {
                int index = ExtractEntityIndex(data[hdrSize + 1], data[hdrSize + 2]);
                if (index > 0 && index <= MaxEntityIndex)
                    RemoveMonster(index);
            }
            else if (data.Length >= hdrSize + 2)
            {
                // Fallback: 1 byte index (alguns servidores podem enviar formato antigo)
                int index = data[hdrSize + 1];
                if (index > 0 && index <= MaxEntityIndex)
                    RemoveMonster(index);
            }
        }

        /// <summary>Distância euclidiana real (com raiz) — só quando precisar do valor.</summary>
        public double Distance(int x, int y)
        {
            if (!HasPosition) return double.MaxValue;
            int dx = x - PlayerX, dy = y - PlayerY;
            return Math.Sqrt(dx * dx + dy * dy);
        }

        /// <summary>Distância ao quadrado — para comparar com range² sem raiz.</summary>
        public int DistanceSquared(int x, int y)
        {
            if (!HasPosition) return int.MaxValue;
            int dx = x - PlayerX, dy = y - PlayerY;
            return dx * dx + dy * dy;
        }

        /// <summary>Retorna os índices das entidades dentro do range (raio).</summary>
        public List<int> EntitiesInRange(int? range = null)
        {
            int r = range ?? Range;
            var result = new List<int>(16);
            if (!HasPosition) return result;
            int rangeSq = r * r;
            int px = PlayerX, py = PlayerY;
            foreach (var kv in _entities)
            {
                int dx = kv.Value.X - px, dy = kv.Value.Y - py;
                if (dx * dx + dy * dy <= rangeSq)
                    result.Add(kv.Key);
            }
            return result;
        }

        /// <summary>Retorna as entidades no range com index, x, y e distância até o jogador.</summary>
        public List<(int Index, int X, int Y, int DistSq)> EntitiesInRangeDetailed(int? range = null)
        {
            lock (_lock) return EntitiesInRangeDetailedLocked(range);
        }

        /// <summary>Versão interna que assume que _lock já está adquirido.</summary>
        private List<(int Index, int X, int Y, int DistSq)> EntitiesInRangeDetailedLocked(int? range = null)
        {
            int r = range ?? Range;
            var result = new List<(int, int, int, int)>(16);
            if (!HasPosition) return result;
            int rangeSq = r * r;
            int px = PlayerX, py = PlayerY;
            foreach (var kv in _entities)
            {
                if (kv.Value.X < 0 || kv.Value.Y < 0) continue;
                int dx = kv.Value.X - px, dy = kv.Value.Y - py;
                int distSq = dx * dx + dy * dy;
                if (distSq <= rangeSq)
                    result.Add((kv.Key, kv.Value.X, kv.Value.Y, distSq));
            }
            result.Sort((a, b) => a.Item4.CompareTo(b.Item4));
            return result;
        }

        /// <summary>Retorna informações de uma entidade pelo index.</summary>
        public bool TryGetEntity(int index, out EntityInfo info) { lock (_lock) return _entities.TryGetValue(index, out info); }

        /// <summary>Número total de entidades registradas (incluindo jogador).</summary>
        public int EntityCount { get { lock (_lock) return _entities.Count; } }

        /// <summary>Número de monstros dentro do range de detecção.</summary>
        public int MonstersInRangeCount
        {
            get
            {
                if (!HasPosition) return 0;
                int r = Range, rangeSq = r * r;
                int px = PlayerX, py = PlayerY, count = 0;
                lock (_lock)
                {
                foreach (var kv in _entities)
                {
                    if (kv.Key == _playerIndex) continue;
                    if (kv.Value.X < 0 || kv.Value.Y < 0) continue;
                    int dx = kv.Value.X - px, dy = kv.Value.Y - py;
                    if (dx * dx + dy * dy <= rangeSq) count++;
                }
                }
                return count;
            }
        }

        /// <summary>Retorna o Monster Class de uma entidade (0 se desconhecido).</summary>
        public int GetMonsterClass(int index) { lock (_lock) return _entities.TryGetValue(index, out var info) ? info.Class : 0; }

        /// <summary>Atualiza ou cria entidade com dados recebidos do byp.dll (OBJ line).</summary>
        public void UpdateEntityFromDll(int index, int compositeIdx, int x, int y, int type, int level, float dist, string name)
        {
            lock (_lock)
            {
            bool existed = _entities.TryGetValue(index, out var prev);
            var info = existed ? prev : new EntityInfo();
            info.X = x;
            info.Y = y;
            info.Class = type;
            if (level != 0) info.Level = (short)level;
            if (!string.IsNullOrEmpty(name)) info.Name = name;

            ushort rawIndex = (ushort)compositeIdx;
            if (rawIndex != 0) info.RawIndex = rawIndex;

            ushort attackIndex = ComputeAttackIndex((ushort)index);
            if (attackIndex != ATTACK_XOR_KEY)
                info.AttackIndex = attackIndex;
            else if (existed && prev.AttackIndex != 0)
                info.AttackIndex = prev.AttackIndex;

            _entities[index] = info;
            }
        }

        /// <summary>Retorna o nome de uma entidade (ou null se desconhecido).</summary>
        public string? GetEntityName(int index) { lock (_lock) return _entities.TryGetValue(index, out var info) ? info.Name : null; }

        /// <summary>
        /// RECV 0xF3 — TargetData (resposta de clique/target).
        /// Sub-opcode 0xE2 = dados de monstro/jogador.
        /// 
        /// Layout MuEmu (PMSG_TARGET_MONSTER_DATA):
        ///   C2 [sizeHi] [sizeLo] F3 E2 [aIndex:4] [isMonster:1] [Life:4] [MaxLife:4] [Level:2]
        ///   
        /// Para C1/C3 (se o servidor usar):
        ///   C1/C3 [size] F3 E2 ...
        /// 
        /// O aIndex é o object index do servidor (int32 little-endian).
        /// isMonster=true indica que o alvo é um monstro/NPC.
        /// Life/MaxLife são int32 little-endian.
        /// Level é int16 little-endian.
        /// </summary>
        private void ParseTargetDataF3(byte[] data)
        {
            // Determina header size
            int hdrSize = data[0] switch
            {
                0xC1 or 0xC3 => 2,
                0xC2 or 0xC4 => 3,
                _ => 2
            };
            if (data.Length < hdrSize + 1) return;

            byte opcode = data[hdrSize];
            if (opcode != 0xF3) return;

            // Precisa do sub-opcode
            if (data.Length < hdrSize + 2) return;
            byte subOpcode = data[hdrSize + 1];

            // Sub-opcode 0xE2 = TargetData
            if (subOpcode != 0xE2) return;

            // Layout após header+opcode+subh:
            // [aIndex:4] [isMonster:1] [Life:4] [MaxLife:4] [Level:2] = 15 bytes
            int payloadOffset = hdrSize + 2; // após type+size+head+subh
            if (data.Length < payloadOffset + 15) return;

            // aIndex: int32 little-endian (formato MuEmu)
            // Layout: [byte0] [byte1] [byte2] [byte3] [isMonster] ...
            // O index real está nos 2 bytes inferiores (byte0, byte1) em little-endian
            int aIndex = BitConverter.ToInt32(data, payloadOffset);
            bool isMonster = data[payloadOffset + 4] != 0;
            int life = BitConverter.ToInt32(data, payloadOffset + 5);
            int maxLife = BitConverter.ToInt32(data, payloadOffset + 9);
            short level = BitConverter.ToInt16(data, payloadOffset + 13);

            // rawIndex = 2 bytes inferiores do aIndex (little-endian, como servidor espera no 0x11)
            // Exemplo: aIndex=0x020000ED -> rawIndex=0xED02
            // Mas pelo seu log, o TargetData tem: 02-ED-02-02 -> aIndex=0x0202ED02
            // Os 2 bytes do index são: data[payloadOffset]=0x02, data[payloadOffset+1]=0xED
            // rawIndex little-endian = (data[payloadOffset+1] << 8) | data[payloadOffset] = 0xED02
            ushort rawIndex = (ushort)(aIndex & 0xFFFF);
            
            // Extrair index real (sem flags) para uso interno
            int indexLo = aIndex & 0xFF;
            int indexHi = (aIndex >> 8) & 0xFF;
            int index = ((indexHi & 0x3F) << 8) | indexLo;

            Log?.Invoke($"[MON] 0xF3-E2 TargetData aIndex=0x{aIndex:X8} idx={index:X4} raw={rawIndex:X4} isMonster={isMonster} life={life}/{maxLife} lvl={level}");

            // Registrar entidade se não estiver no tracker
            if (isMonster && index > 0 && index <= MaxEntityIndex)
            {
                if (!_entities.ContainsKey(index))
                {
                    Log?.Invoke($"[MON] 0xF3-E2 registrando idx={index:X4} raw={rawIndex:X4} (TargetData, sem posição)");
                    // Registrar com rawIndex do TargetData
                    var newInfo = new EntityInfo
                    {
                        X = -1,
                        Y = -1,
                        Class = 0,
                        RawIndex = rawIndex,
                        Level = level,
                        MaxLife = maxLife,
                        Life = life
                    };
                    _entities[index] = newInfo;
                }
                // Atualizar dados do monstro existente (EntityInfo é struct, precisa reatribuir)
                if (_entities.TryGetValue(index, out var existingInfo))
                {
                    existingInfo.Level = level;
                    existingInfo.MaxLife = maxLife;
                    existingInfo.Life = life;
                    // Atualizar rawIndex se for diferente (prioriza TargetData)
                    if (existingInfo.RawIndex != rawIndex)
                    {
                        existingInfo.RawIndex = rawIndex;
                        Log?.Invoke($"[MON] 0xF3-E2 atualizou rawIndex para {rawIndex:X4}");
                    }
                    _entities[index] = existingInfo; // reatribuir struct modificado ao dicionário
                    Log?.Invoke($"[MON] 0xF3-E2 atualizou idx={index:X4} lvl={level} life={life}/{maxLife} raw={rawIndex:X4} attackIdx={existingInfo.AttackIndex:X4}");
                }
            }
        }

        /// <summary>
        /// RECV 0xF1-0x00 — Login/Connect Client Result (GCConnectClientSend).
        /// O servidor envia este pacote quando o jogador entra no jogo com sucesso.
        /// Contém o player index (aIndex) que é essencial para diferenciar
        /// movimentos do jogador de movimentos de outras entidades no RECV D4.
        /// 
        /// Formato (PSBMSG_HEAD = C1 com sub-opcode):
        ///   C1 [size] F1 00 [result] [indexHi] [indexLo] [ClientVersion x5]
        ///   
        /// result = 1 significa sucesso. index = (indexHi &lt;&lt; 8) | indexLo = player aIndex.
        /// O player index pode ter flags nos bits altos (bit 7 = OBJECT_CREATE, bit 6 = teleporting).
        /// </summary>
        private void ParseLoginF1(byte[] data)
        {
            // PSBMSG_HEAD: type(1) + size(1) + head(1) + subh(1) = 4 bytes (C1)
            // PSWMSG_HEAD: type(1) + sizeHi(1) + sizeLo(1) + head(1) + subh(1) = 5 bytes (C2)
            if (data.Length < 4) return;

            byte headerType = data[0];
            int hdrSize = headerType switch
            {
                0xC1 or 0xC3 => 2,
                0xC2 or 0xC4 => 3,
                _ => 2
            };

            byte opcode = data[hdrSize]; // 0xF1
            if (opcode != 0xF1) return;

            // Sub-opcode está logo após o opcode (head)
            // C1: data[2] = head, data[3] = subh
            // C2: data[3] = head, data[4] = subh
            int subPos = hdrSize + 1;
            if (data.Length < subPos + 1) return;
            byte subOpcode = data[subPos];

            // Apenas sub-opcode 0x00 (GCConnectClient / login success)
            if (subOpcode != 0x00) return;

            // Layout após PSBMSG_HEAD (4 bytes para C1, 5 bytes para C2):
            // [result:1] [indexHi:1] [indexLo:1] [ClientVersion:5]
            // Para C1: payload começa em data[4]
            // Para C2: payload começa em data[5]
            int payloadStart = (headerType == 0xC2 || headerType == 0xC4) ? 5 : 4;
            if (data.Length < payloadStart + 3) return; // precisa de result + indexHi + indexLo

            byte result = data[payloadStart];
            if (result != 1) // result=1 = login success
            {
                Log?.Invoke($"[MON] 0xF1-00 Login result={result} (não é sucesso, ignorando)");
                return;
            }

            byte indexHi = data[payloadStart + 1];
            byte indexLo = data[payloadStart + 2];
            int playerIndex = (indexHi << 8) | indexLo;

            _playerIndex = playerIndex;
            PlayerIndexChanged?.Invoke(playerIndex);
            Log?.Invoke($"[MON] 0xF1-00 Login SUCCESS playerIndex={playerIndex:X4} hi={indexHi:X2} lo={indexLo:X2}");

            // Também registrar o jogador como entidade conhecida
            if (playerIndex > 0 && playerIndex <= MaxEntityIndex)
            {
                if (!_entities.ContainsKey(playerIndex))
                {
                    Log?.Invoke($"[MON] 0xF1-00 registrando player idx={playerIndex:X4} (sem posição)");
                    RegisterEntity(playerIndex, -1, -1);
                }
            }
        }

        /// <summary>Retorna o RawIndex (2 bytes com flags) de uma entidade (0 se desconhecido).
        /// Este é o valor que deve ser enviado nos pacotes de ataque/movimento.</summary>
        public ushort GetRawIndex(int index) => _entities.TryGetValue(index, out var info) ? info.RawIndex : (ushort)0;

        public void Clear() => _entities.Clear();
    }
}
