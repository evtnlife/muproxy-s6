using WinFormsMitm.Protocol;

namespace WinFormsMitm.Services
{
    /// <summary>
    /// Serviço de rastreamento de monstros e posição do jogador.
    /// Recebe dados do byp.dll (ENTITY lines) e de pacotes do jogo (D4/F1).
    /// </summary>
    public class MonsterService
    {
        private readonly DllCommandService _dll;
        private readonly Action<string>? _log;

        // ---- Dados ----
        public List<MonsterInfo> Monsters { get; private set; } = new();
        public int PlayerX { get; private set; }
        public int PlayerY { get; private set; }
        public int PlayerIndex { get; private set; }
        public bool HasPosition { get; private set; }

        // ---- Eventos para UI ----
        public event Action? OnMonstersUpdated;
        public event Action<int, int>? OnPlayerPositionChanged;
        public event Action<int>? OnPlayerIndexChanged;

        public MonsterService(DllCommandService dll, Action<string>? log = null)
        {
            _dll = dll;
            _log = log;

            // Assina lista de monstros do byp.dll
            _dll.OnMonsterList += OnDllMonsterList;
        }

        private void OnDllMonsterList(List<EntityMessage> entities)
        {
            const int ATTACK_XOR_KEY = 0xEFF5;
            var list = new List<MonsterInfo>(entities.Count);
            foreach (var e in entities)
            {
                list.Add(new MonsterInfo
                {
                    SlotIndex = e.Slot,
                    CompositeIndex = e.CompositeIndex,
                    AttackId = e.CompositeIndex ^ ATTACK_XOR_KEY,
                    Type = e.Type,
                    Level = e.Level,
                    X = (int)(e.X / 100f),
                    Y = (int)(e.Y / 100f),
                    Distance = e.Distance / 100f,
                    Name = e.Name,
                    TypeName = e.Type switch
                    {
                        1 => "Player",
                        2 => "Monster",
                        4 => "NPC",
                        _ => $"Type{e.Type}"
                    }
                });
            }
            Monsters = list;
            OnMonstersUpdated?.Invoke();
        }

        /// <summary>Processa pacote de posição do jogo (chamado pelo pipeline de pacotes).</summary>
        public void OnPositionPacket(byte[] data, string dir)
        {
            if (data == null || data.Length < 4) return;
            byte header = data[0];
            int hdrSize = header switch { 0xC1 or 0xC3 => 2, 0xC2 or 0xC4 => 3, _ => 0 };
            if (hdrSize == 0 || data.Length <= hdrSize) return;

            byte opcode = data[hdrSize];
            bool isSend = dir.Contains("SEND") || dir.Contains("C->S");

            // SEND D4: posição destino do clique/walk do jogador
            if (opcode == 0xD4 && isSend && data.Length >= 5)
            {
                SetPlayerPosition(data[3], data[4]);
            }
            // RECV D4: servidor confirma posição
            else if (opcode == 0xD4 && !isSend && data.Length >= 8)
            {
                byte idxHi = data[3], idxLo = data[4];
                int index = ((idxHi & 0x3F) << 8) | idxLo;
                int x = data[5], y = data[6];

                // Se é o player index conhecido, atualiza posição
                if (PlayerIndex > 0 && index == PlayerIndex)
                {
                    SetPlayerPosition(x, y);
                }
                // Auto-detecção: se posição próxima do último SEND D4
                else if (HasPosition && PlayerIndex == 0)
                {
                    int dx = x - PlayerX, dy = y - PlayerY;
                    if (dx * dx + dy * dy <= 9)
                    {
                        PlayerIndex = index;
                        OnPlayerIndexChanged?.Invoke(index);
                        _dll.SendCommand(new AttackPlayerIdxMessage { PlayerIndex = index }.ToWire());
                        _log?.Invoke($"[MON] PlayerIndex detectado: 0x{index:X4}");
                    }
                }
            }
            // RECV F1: login, extrai player index
            else if (opcode == 0xF1 && !isSend && data.Length >= hdrSize + 4)
            {
                byte subcode = data[hdrSize + 1];
                if (subcode == 0x00 && data.Length >= hdrSize + 6)
                {
                    int index = data[hdrSize + 4] | (data[hdrSize + 5] << 8);
                    if (index > 0 && index != PlayerIndex)
                    {
                        PlayerIndex = index;
                        OnPlayerIndexChanged?.Invoke(index);
                        _dll.SendCommand(new AttackPlayerIdxMessage { PlayerIndex = index }.ToWire());
                        _log?.Invoke($"[MON] PlayerIndex via login: 0x{index:X4}");
                    }
                }
            }
        }

        private void SetPlayerPosition(int x, int y)
        {
            PlayerX = x;
            PlayerY = y;
            HasPosition = true;
            OnPlayerPositionChanged?.Invoke(x, y);
        }
    }

    public class MonsterInfo
    {
        public int SlotIndex { get; set; }
        public int CompositeIndex { get; set; }
        public int AttackId { get; set; }       // CompositeIndex XOR 0xEFF5 (crypted attack target ID)
        public int Type { get; set; }
        public int Level { get; set; }
        public int X { get; set; }
        public int Y { get; set; }
        public float Distance { get; set; }
        public string Name { get; set; } = "";
        public string TypeName { get; set; } = "";
    }
}
