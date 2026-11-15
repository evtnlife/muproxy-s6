using System.Diagnostics;
using System.Net;
using System.Net.Sockets;
using System.Runtime.InteropServices;
using System.Text;
using WinFormsMitm.Events;
using WinFormsMitm.Protocol;
using WinFormsMitm.Services;

namespace WinFormsMitm
{
    public partial class MainForm : Form
    {
        // ---- Serviços ----
        private DllCommandService? _dll;
        private HookListenerService? _hookListener;
        private MonsterService? _monsterService;
        private AttackService? _attackService;

        private TcpLoopbackProxy? _proxy;

        HashSet<string> bypassed = new HashSet<string>();

        const string GMO_PATH_DEFAULT = @"F:\blackrockmu\main.exe";
        private string _exePath = GMO_PATH_DEFAULT;
        private string _serverHost = "54.36.205.234";
        private int _serverListPort = 44411;
        // Porta do GS e detectada automaticamente via pacote C116 (ConnectToServer)
        // quando o login server a revela — nao ha mais campo fixo na UI.

        [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
        static extern bool CreateProcess(string? appName, string? cmdLine, IntPtr procAttr, IntPtr threadAttr,
            bool inherit, uint flags, IntPtr env, string? cwd,
            ref STARTUPINFO si, out PROCESS_INFORMATION pi);
        [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr h);

        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
        struct STARTUPINFO
        {
            public int cb; public string lpReserved, lpDesktop, lpTitle;
            public int dwX, dwY, dwXSize, dwYSize, dwXCountChars, dwYCountChars, dwFillAttribute, dwFlags;
            public short wShowWindow, cbReserved2;
            public IntPtr lpReserved2, hStdInput, hStdOutput, hStdError;
        }
        [StructLayout(LayoutKind.Sequential)]
        struct PROCESS_INFORMATION { public IntPtr hProcess, hThread; public int dwProcessId, dwThreadId; }

        // Log filter checkboxes
        private CheckBox chkHook = null!, chkRaw = null!, chkSend = null!, chkMon = null!, chkAtk = null!, chkOther = null!;

        private StreamWriter? _logFile;
        private StreamWriter? _packetLogFile;
        private readonly object _packetLogLock = new();
        private readonly object _logFileLock = new();
        private const int MAX_LOG_LINES = 100;
        private readonly Queue<LogEvent> _uiLogQueue = new();
        private bool _logDirty = false;
        private System.Windows.Forms.Timer? _logThrottleTimer;

        public MainForm()
        {
            InitializeComponent();
            FormClosing += MainForm_FormClosing;

            // Inicializa serviços
            _dll = new DllCommandService();
            _dll.OnLog += msg => Log(new LogEvent(DateTime.Now, msg));
            _dll.OnPacketSniffed += (dir, data) => AddPacket(dir, data);

            _monsterService = new MonsterService(_dll, msg => Log(new LogEvent(DateTime.Now, msg)));
            _monsterService.OnMonstersUpdated += UpdateMonsterListView;
            _monsterService.OnPlayerPositionChanged += (x, y) =>
            {
                if (InvokeRequired) { BeginInvoke(() => UpdatePlayerCoordStatus(x, y)); return; }
                UpdatePlayerCoordStatus(x, y);
            };
            _monsterService.OnPlayerIndexChanged += idx =>
            {
                if (InvokeRequired) { BeginInvoke(UpdatePlayerInfo); return; }
                UpdatePlayerInfo();
            };

            _attackService = new AttackService(_dll, msg => Log(new LogEvent(DateTime.Now, msg)));
            _attackService.OnStateChanged += enabled =>
            {
                if (InvokeRequired) { BeginInvoke(() => UpdateAttackUI(enabled)); return; }
                UpdateAttackUI(enabled);
            };

            _hookListener = new HookListenerService(_dll, msg => Log(new LogEvent(DateTime.Now, msg)));

            SetupLogFilters();
            OpenLogFile();
            LogPacketCatalogStatus();
            SetupAttackControls();
            StartHookServer();

            // Timer para throttle do RebuildLogUI
            _logThrottleTimer = new System.Windows.Forms.Timer { Interval = 150 };
            _logThrottleTimer.Tick += (s, e) => { if (_logDirty) { _logDirty = false; RebuildLogUI(); } };
            _logThrottleTimer.Start();

            // Ctrl+C na lista de monstros
            lstMonsters.KeyDown += (s, e) =>
            {
                if (e.Control && e.KeyCode == Keys.C)
                {
                    var sb = new StringBuilder();
                    foreach (ListViewItem item in lstMonsters.SelectedItems)
                    {
                        var cols = new string[item.SubItems.Count];
                        for (int i = 0; i < item.SubItems.Count; i++)
                            cols[i] = item.SubItems[i].Text;
                        sb.AppendLine(string.Join("\t", cols));
                    }
                    if (sb.Length > 0) Clipboard.SetText(sb.ToString());
                }
            };
        }

        // ============ UI: Tabela de monstros ============

        private void UpdateMonsterListView()
        {
            if (InvokeRequired) { BeginInvoke(UpdateMonsterListView); return; }
            if (lstMonsters == null || _monsterService == null) return;

            lstMonsters.BeginUpdate();
            try
            {
                lstMonsters.Items.Clear();
                foreach (var m in _monsterService.Monsters)
                {
                    int range = (int)Math.Ceiling(m.Distance);
                    var item = new ListViewItem(new[]
                    {
                        m.SlotIndex.ToString("X4"),
                        m.Name,
                        m.TypeName,
                        m.Level > 0 ? m.Level.ToString() : "-",
                        range.ToString(),
                        m.AttackId.ToString("X4"),
                        $"{m.X},{m.Y}"
                    });
                    lstMonsters.Items.Add(item);
                }
            }
            finally
            {
                lstMonsters.EndUpdate();
            }
            UpdatePlayerInfo();
        }

        private void UpdatePlayerCoordStatus(int x, int y)
        {
            toolTipPlayerCoord.Text = $"Player: ({x}, {y})";
            UpdatePlayerInfo();
        }

        private void UpdatePlayerInfo()
        {
            if (_monsterService == null) return;
            var sb = new StringBuilder();
            sb.Append($"Idx:0x{_monsterService.PlayerIndex:X4}");
            sb.Append($" | Pos:({_monsterService.PlayerX},{_monsterService.PlayerY})");
            sb.Append($" | Monsters:{_monsterService.Monsters.Count}");
            toolTipPlayerInfo.Text = sb.ToString();
        }

        private void UpdateAttackUI(bool enabled)
        {
            chkEnableAttack.Checked = enabled;
        }

        // ============ UI: Controles de ataque ============

        private void SetupAttackControls()
        {
            if (_attackService == null) return;

            txtRange.Text = _attackService.Range.ToString();
            txtDelay.Text = _attackService.DelayMs.ToString();
            txtHitCount.Text = _attackService.HitCount.ToString();

            chkEnableAttack.CheckedChanged += (s, e) =>
            {
                if (_attackService == null) return;
                if (chkEnableAttack.Checked)
                    _attackService.Start();
                else
                    _attackService.Stop();
            };

            txtRange.TextChanged += (s, e) =>
            {
                if (int.TryParse(txtRange.Text, out int r) && r > 0 && _attackService != null)
                {
                    _attackService.Range = r;
                    _attackService.SendConfig();
                }
            };

            txtDelay.TextChanged += (s, e) =>
            {
                if (int.TryParse(txtDelay.Text, out int ms) && ms >= 50 && _attackService != null)
                {
                    _attackService.DelayMs = ms;
                    _attackService.SendConfig();
                }
            };

            txtHitCount.TextChanged += (s, e) =>
            {
                if (int.TryParse(txtHitCount.Text, out int hc) && hc >= 1 && _attackService != null)
                {
                    _attackService.HitCount = hc;
                    _attackService.SendConfig();
                }
            };

            rbNormalAttack.CheckedChanged += (s, e) =>
            {
                if (rbNormalAttack.Checked)
                {
                    cbSkills.Enabled = false;
                    _attackService?.SetNormalAttack();
                }
            };

            rbSkill.CheckedChanged += (s, e) =>
            {
                cbSkills.Enabled = rbSkill.Checked;
                if (rbSkill.Checked && cbSkills.SelectedItem is SkillOption skill)
                    _attackService?.SetSkill(skill.Action);
            };

            cbSkills.SelectedIndexChanged += (s, e) =>
            {
                if (rbSkill.Checked && cbSkills.SelectedItem is SkillOption skill)
                    _attackService?.SetSkill(skill.Action);
            };

            PopulateSkills(cbSkills);
            rbNormalAttack.Checked = true;
        }

        // ============ UI: Skills ============

        // Tabela de nomes de skills — IDs extraídos das constantes do eMU
        // (MuEmu-master/eMU/GameServer/GameServer/SkillManager.h, enum a partir de SKILL_POISON).
        // O campo "skill" do pacote F3 11 é exatamente esse número (m_index).
        private static readonly Dictionary<ushort, string> SkillNames = new()
        {
            [1] = "Poison",
            [2] = "Meteorite",
            [3] = "Lightning",
            [4] = "Fire Ball",
            [5] = "Flame",
            [6] = "Teleport",
            [7] = "Ice",
            [8] = "Twister",
            [9] = "Evil Spirit",
            [10] = "Hell Fire",
            [11] = "Power Wave",
            [12] = "Aqua Beam",
            [13] = "Blast",
            [14] = "Inferno",
            [15] = "Teleport (Ally)",
            [16] = "Mana Shield",
            [17] = "Energy Ball",
            [18] = "Defense",
            [19] = "Falling Slash",
            [20] = "Lunge",
            [21] = "Uppercut",
            [22] = "Cyclone",
            [23] = "Slash",
            [24] = "Triple Shot",
            [26] = "Heal",
            [27] = "Greater Defense",
            [28] = "Greater Damage",
            [30] = "Summon 1",
            [31] = "Summon 2",
            [32] = "Summon 3",
            [33] = "Summon 4",
            [34] = "Summon 5",
            [35] = "Summon 6",
            [36] = "Summon 7",
            [38] = "Decay",
            [39] = "Ice Storm",
            [40] = "Nova",
            [41] = "Twisting Slash",
            [42] = "Rageful Blow",
            [43] = "Death Stab",
            [44] = "Crescent Moon Slash",
            [45] = "Mana Glaive",
            [46] = "Star Fall",
            [47] = "Impale",
            [48] = "Greater Life",
            [49] = "Fire Breath",
            [50] = "Monster Area Attack",
            [51] = "Ice Arrow",
            [52] = "Penetration",
            [55] = "Fire Slash",
            [56] = "Power Slash",
            [57] = "Spiral Slash",
            [58] = "Nova Start",
            [59] = "Combo",
            [60] = "Force",
            [61] = "Fire Burst",
            [62] = "Earthquake",
            [63] = "Summon Party",
            [64] = "Greater Critical Damage",
            [65] = "Electric Spark",
            [66] = "Force Wave",
            [67] = "Stern",
            [68] = "Remove Stern",
            [69] = "Greater Mana",
            [70] = "Invisibility",
            [71] = "Remove Invisibility",
            [72] = "Remove All Effect",
            [73] = "Mana Rays",
            [74] = "Fire Blast",
            [75] = "Brand",
            [76] = "Plasma Storm",
            [77] = "Infinity Arrow",
            [78] = "Fire Scream",
            [79] = "Explosion",
            [200] = "Monster Summon",
            [201] = "Magic Damage Immunity",
            [202] = "Physi Damage Immunity",
            [210] = "Order of Protection",
            [211] = "Order of Restraint",
            [212] = "Order of Tracking",
            [213] = "Order of Weaken",
            [214] = "Drain Life",
            [215] = "Chain Lightning",
            [216] = "Electric Surge",
            [217] = "Damage Reflect",
            [218] = "Sword Power",
            [219] = "Sleep",
            [221] = "Lesser Damage",
            [222] = "Lesser Defense",
            [223] = "Sahamutt",
            [224] = "Neil",
            [225] = "Ghost Phantom",
            [230] = "Red Storm",
            [232] = "Frozen Stab",
            [233] = "Magic Circle",
            [234] = "Shield Recover",
            [235] = "Five Shot",
            [236] = "Sword Slash",
            [237] = "Lightning Storm",
            [238] = "Birds",
            [239] = "Self Explosion",
            [250] = "Raklion Selupan 1",
            [251] = "Raklion Selupan 2",
            [252] = "Raklion Selupan 3",
            [253] = "Raklion Selupan 4",
            [260] = "Large Ring Blower",
            [261] = "Upper Beast",
            [262] = "Chain Driver",
            [263] = "Dark Side",
            [264] = "Dragon Lore",
            [265] = "Dragon Slayer",
            [266] = "Greater Ignore Defense Rate",
            [267] = "Fitness",
            [268] = "Greater Defense Success Rate",
            [269] = "Charge",
            [270] = "Phoenix Shot",
            [344] = "Blood Storm",
            [425] = "Cure",
            [426] = "Party Heal",
            [427] = "Poison Arrow",
            [430] = "Bless",
            [432] = "Summon Satyros",
            [461] = "Blind",
            [495] = "Earth Prison",
            [521] = "Iron Defense",
            [565] = "Blood Howling",
        };

        private static string GetSkillName(ushort skillId, ushort level)
        {
            if (SkillNames.TryGetValue(skillId, out var name))
                return $"{name} (Lv{level})";
            return $"Skill 0x{skillId:X4} (Lv{level})";
        }

        private void OnSkillListReceived(byte[] data, byte subcode)
        {
            int hdrSize = data[0] switch { 0xC1 or 0xC3 => 2, 0xC2 or 0xC4 => 3, _ => 0 };
            int dataStart = hdrSize + 2; // após header + opcode F3 + subcode

            // F3 11 (SkillList): [count][listType][entry*4]
            // entry: [0] slot(1B), [1..2] m_index(2B LE), [3] level_encoded(1B)
            // level_encoded = (m_level << 3) | ((m_index/255) & 7)
            // → actual level = level_encoded >> 3
            if (subcode == 0x11)
            {
                int count = data[dataStart];
                byte listType = data[dataStart + 1]; // 0=clear+add, 2=remove, other=add
                int entryStart = dataStart + 2;
                const int ENTRY_SIZE = 4;

                var skills = new List<SkillOption>();
                int maxEntries = Math.Min(count, (data.Length - entryStart) / ENTRY_SIZE);
                for (int i = 0; i < maxEntries; i++)
                {
                    int off = entryStart + i * ENTRY_SIZE;
                    if (off + ENTRY_SIZE > data.Length) break;

                    byte slot = data[off];
                    ushort skillId = (ushort)(data[off + 1] | (data[off + 2] << 8));
                    byte level = (byte)(data[off + 3] >> 3); // decode: level is in high 5 bits
                    if (skillId == 0 || skillId == 0xFFFF) continue;
                    skills.Add(new SkillOption(GetSkillName(skillId, level), skillId));
                }

                if (skills.Count == 0) return;
                if (InvokeRequired)
                    BeginInvoke(() => MergeSkillCombo(cbSkills, skills));
                else
                    MergeSkillCombo(cbSkills, skills);
                Log(new LogEvent(DateTime.Now, $"[SKILL] F3 11: {skills.Count} skills (listType={listType})"));
                return;
            }

            // F3 E2 (SkillCast): eventos de uso com posição.
            // entry: [0..1] skillId(2B LE), [2..3] flags, [4..19] pos(4 floats), [20..21] level(2B LE)
            //
            // ATENÇÃO: NÃO popola o combo a partir do F3 E2. O F3 E2 traz skills de
            // TODAS as entidades do viewport (monstros e outros jogadores lançando
            // skills), não apenas as skills do próprio personagem. Isso fazia skills
            // que o personagem NÃO possui aparecerem no combo. A fonte confiável das
            // skills do personagem é o F3 11 (SkillList) acima. Aqui só logamos.
            if (subcode == 0xE2)
            {
                int count = data[dataStart];
                int entryStart = dataStart + 1;
                const int ENTRY_SIZE = 24;

                int casts = 0;
                int maxEntries = Math.Min(count, (data.Length - entryStart) / ENTRY_SIZE);
                for (int i = 0; i < maxEntries; i++)
                {
                    int off = entryStart + i * ENTRY_SIZE;
                    if (off + ENTRY_SIZE > data.Length) break;

                    ushort skillId = (ushort)(data[off] | (data[off + 1] << 8));
                    if (skillId == 0 || skillId == 0xFFFF) continue;
                    casts++;
                }

                if (casts > 0)
                    Log(new LogEvent(DateTime.Now, $"[SKILL] F3 E2: {casts} casts observados (não adicionados ao combo)"));
            }
        }

        /// <summary>Merge skills no combo sem duplicar (preserva seleção atual).</summary>
        private void MergeSkillCombo(ComboBox cb, List<SkillOption> newSkills)
        {
            var existing = new HashSet<int>();
            foreach (var item in cb.Items)
                if (item is SkillOption so) existing.Add(so.Action);

            int added = 0;
            foreach (var s in newSkills)
            {
                if (existing.Contains(s.Action)) continue;
                cb.Items.Add(s);
                existing.Add(s.Action);
                added++;
            }

            if (added > 0 && cb.SelectedIndex < 0) cb.SelectedIndex = 0;
        }

        private void PopulateSkills(ComboBox cbSkills)
        {
            cbSkills.Items.Clear();
            cbSkills.Items.Add(new SkillOption("Aguardando skills...", 0));
            cbSkills.DisplayMember = "Name";
            cbSkills.ValueMember = "Name";
            cbSkills.SelectedIndex = 0;
        }

        // Action = SkillNumber (2B LE do F3 11) — o MESMO valor que o AttackModule
        // codifica no pacote C1 07 19 [skillH][skillL] (ex: Force = 60 = 0x003C).
        // 0 = ainda sem skill (byp.dll usa o defaultSkillId).
        private record SkillOption(string Name, int Action)
        {
            public override string ToString() => Name;
        }

        // ============ UI: Log ============

        private static CheckBox MkChk(string text, bool chk)
            => new CheckBox { Text = text, Checked = chk, AutoSize = true, Margin = new Padding(6, 4, 0, 0) };

        private void OpenLogFile()
        {
            try
            {
                var dir = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "logs");
                Directory.CreateDirectory(dir);
                var path = Path.Combine(dir, $"muproxy2_{DateTime.Now:yyyyMMdd_HHmmss}.log");
                _logFile = new StreamWriter(path, true) { AutoFlush = true };

                var pktPath = Path.Combine(dir, $"hook_packets_{DateTime.Now:yyyyMMdd_HHmmss}.csv");
                _packetLogFile = new StreamWriter(pktPath, true) { AutoFlush = true };
                _packetLogFile.WriteLine("timestamp,dir,length,type,hex");
            }
            catch { }
        }

        private void Log(LogEvent msg)
        {
            var logFile = _logFile;
            if (logFile != null)
            {
                var line = $"[{msg.Timestamp:HH:mm:ss.fff}] {msg.Message}";
                lock (_logFileLock) { try { logFile.WriteLine(line); } catch { } }
            }

            if (InvokeRequired) { BeginInvoke(() => Log(msg)); return; }
            if (!ShouldShowLog(msg.Message)) return;

            _uiLogQueue.Enqueue(msg);
            while (_uiLogQueue.Count > MAX_LOG_LINES) _uiLogQueue.Dequeue();
            _logDirty = true;
        }

        private void RebuildLogUI()
        {
            if (InvokeRequired) { BeginInvoke(RebuildLogUI); return; }
            var sb = new StringBuilder();
            foreach (var m in _uiLogQueue)
                sb.AppendLine($"[{m.Timestamp:HH:mm:ss}] {m.Message}");
            txtLog.Text = sb.ToString();
            txtLog.SelectionStart = txtLog.TextLength;
            txtLog.ScrollToCaret();
        }

        private bool ShouldShowLog(string msg)
        {
            if (msg.Contains("[HOOK]")) return chkHook.Checked;
            if (msg.Contains("[RAW]")) return chkRaw.Checked;
            if (msg.Contains("[SEND]")) return chkSend.Checked;
            if (msg.Contains("[MON]")) return chkMon.Checked;
            if (msg.Contains("[ATK]")) return chkAtk.Checked;
            return chkOther.Checked;
        }

        private void LogPacketCatalogStatus()
        {
            try
            {
                var defs = PacketCatalog.Definitions;
                var types = string.Join(", ", PacketCatalog.KnownTypes());
                Log(new LogEvent(DateTime.Now, $"[CATALOG] Registrados {defs.Count} pacotes. Tipos: {types}"));
            }
            catch (Exception ex)
            {
                Log(new LogEvent(DateTime.Now, $"[CATALOG] ERRO ao carregar catalogo: {ex}"));
            }
        }

        private void SetupLogFilters()
        {
            var panel = new FlowLayoutPanel
            {
                Dock = DockStyle.Top, Height = 28, AutoSize = false,
                WrapContents = false, BackColor = System.Drawing.Color.FromArgb(240, 240, 240)
            };
            var lbl = new Label { Text = "Filtros:", AutoSize = true, Margin = new Padding(4, 6, 0, 0) };
            panel.Controls.Add(lbl);

            chkHook = MkChk("HOOK", false);
            chkRaw = MkChk("RAW", false);
            chkSend = MkChk("SEND", false);
            chkMon = MkChk("MON", false);
            chkAtk = MkChk("ATK", false);
            chkOther = MkChk("Outros", false);

            panel.Controls.AddRange(new Control[] { chkHook, chkRaw, chkSend, chkMon, chkAtk, chkOther });
            tabLogs.Controls.Add(panel);
            panel.BringToFront();
            SetupPacketTypeFilter();
        }

        private void SetupPacketTypeFilter()
        {
            if (txtTypeFilter == null) return;
            txtTypeFilter.Text = "ALL";
            txtTypeFilter.TextChanged += (s, e) => lvPackets.Items.Clear();
        }

        private bool MatchesTypeFilter(PacketType type)
        {
            var filter = txtTypeFilter?.Text?.Trim() ?? "ALL";
            if (string.IsNullOrEmpty(filter)) filter = "ALL";
            if (filter.Equals("ALL", StringComparison.OrdinalIgnoreCase)) return true;
            if (filter.Equals("UNKNOWN", StringComparison.OrdinalIgnoreCase)) return type == PacketType.Unknown;
            return type.ToString().IndexOf(filter, StringComparison.OrdinalIgnoreCase) >= 0;
        }

        // ============ UI: Packet List ============

        private void AddPacket(string dir, byte[] data)
        {
            // Processa posição do jogador em background
            _monsterService?.OnPositionPacket(data, dir);

            if (InvokeRequired)
            {
                string wire = BitConverter.ToString(data);
                if (bypassed.Contains(wire)) return;

                bool canAdd = false;
                if (cbC1.Checked && wire.StartsWith("C1")) canAdd = true;
                if (cbC2.Checked && wire.StartsWith("C2")) canAdd = true;
                if (cbC3.Checked && wire.StartsWith("C3")) canAdd = true;
                if (cbC4.Checked && wire.StartsWith("C4")) canAdd = true;
                if (!canAdd && cbOther.Checked) canAdd = true;
                if (!canAdd) return;

                var def = PacketCatalog.Identify(data, dir);
                string typeName = def.Name;

                int hdrSz = data[0] switch { 0xC1 or 0xC3 => 2, 0xC2 or 0xC4 => 3, _ => 0 };
                byte opcode = hdrSz > 0 && data.Length > hdrSz ? data[hdrSz] : (byte)0;
                byte subcode = (opcode == 0xF3 || opcode == 0xF4) && data.Length > hdrSz + 1 ? data[hdrSz + 1] : (byte)0;

                if (opcode == 0xF3 && (subcode == 0x11 || subcode == 0xE2) && dir == "RECV")
                {
                    int minLen = subcode == 0x11 ? 8 : 30;
                    if (data.Length >= minLen) OnSkillListReceived(data, subcode);
                }

                if (!MatchesTypeFilter(def.Type)) return;

                string plain = BytesToAscii(data);
                string timestamp = DateTime.Now.ToString("HH:mm:ss.fff");

                var pktLogFile = _packetLogFile;
                if (pktLogFile != null)
                {
                    var csvLine = $"{DateTime.Now:yyyy-MM-dd HH:mm:ss.fff},{dir.Replace(",", ";")},{data.Length},{typeName},{wire}";
                    lock (_packetLogLock) { try { pktLogFile.WriteLine(csvLine); } catch { } }
                }

                BeginInvoke(() =>
                {
                    var item = new ListViewItem(new[] { dir, timestamp, data.Length.ToString(), typeName, wire, plain });
                    lvPackets.Items.Add(item);
                    if (cbAutoScroll.Checked) item.EnsureVisible();
                    while (lvPackets.Items.Count > 5000) lvPackets.Items.RemoveAt(0);
                });
                return;
            }
        }

        private static IEnumerable<byte[]> SplitMuPackets(byte[] payload)
        {
            int offset = 0;
            while (offset < payload.Length)
            {
                byte h = payload[offset];
                int hdrSize = h switch { 0xC1 or 0xC3 => 2, 0xC2 or 0xC4 => 3, _ => 0 };
                if (hdrSize == 0) { offset++; continue; }
                if (offset + hdrSize > payload.Length) yield break;

                int pktLen = h switch
                {
                    0xC1 or 0xC3 => payload[offset + 1],
                    0xC2 or 0xC4 => (payload[offset + 1] << 8) | payload[offset + 2],
                    _ => 0
                };
                if (pktLen < hdrSize || offset + pktLen > payload.Length) yield break;

                var pkt = new byte[pktLen];
                Buffer.BlockCopy(payload, offset, pkt, 0, pktLen);
                yield return pkt;
                offset += pktLen;
            }
        }

        // ============ Hook Server ============

        private void StartHookServer()
        {
            _hookListener?.Start();
            // Ativa scan de monstros no byp.dll após conexão
            _dll!.OnDllConnected += () =>
            {
                Task.Run(async () =>
                {
                    await Task.Delay(500);
                    _dll.SendCommand(new MonsterStartMessage { Interval = 500, Filter = 6 }.ToWire());
                });
            };
        }

        private void StopHookServer()
        {
            _hookListener?.Stop();
        }

        // ============ Form Lifecycle ============

        private void MainForm_FormClosing(object? sender, FormClosingEventArgs e)
        {
            _logThrottleTimer?.Stop();
            _logThrottleTimer?.Dispose();
            StopHookServer();
            _dll?.Dispose();
            _logFile?.Dispose();
            _packetLogFile?.Dispose();
        }

        // ============ Proxy ============

        private void btnStart_Click(object? sender, EventArgs e)
        {
            if (_proxy?.IsRunning == true)
            {
                try { _proxy.Stop(); } catch (Exception ex) { Log(new LogEvent(DateTime.Now, $"[PROXY] Erro ao parar: {ex.Message}")); }
                _proxy = null;
                btnStart.Text = "Start Proxy";
                lblStatus.Text = "Status: Stopped";
                Log(new LogEvent(DateTime.Now, "[PROXY] Proxy parado."));
                return;
            }

            if (!int.TryParse(txtListenPort.Text.Trim(), out int serverListPort))
            { Log(new LogEvent(DateTime.Now, "[PROXY] Server List Port inválido")); return; }

            _serverHost = txtRealHost.Text.Trim();
            _serverListPort = serverListPort;

            _proxy = new TcpLoopbackProxy();
            _proxy.OnLog += msg => Log(new LogEvent(DateTime.Now, $"[PROXY] {msg}"));
            _proxy.AddListener(_serverListPort, _serverHost, _serverListPort, rewriteServerList: true);

            // C116 (ConnectToServer) revela a porta real do GS quando o login
            // server a envia. Cria o listener do GS dinamicamente nesse momento
            // — assim o relay existe antes do cliente (byp.dll) conectar no GS.
            var proxyRef = _proxy;
            proxyRef.OnGsPortDetected += gsPort =>
            {
                if (proxyRef.HasListener(gsPort)) return;
                proxyRef.AddListener(gsPort, _serverHost, gsPort, rewriteServerList: false);
                Log(new LogEvent(DateTime.Now, $"[PROXY] GS: listener criado 127.0.0.1:{gsPort} -> {_serverHost}:{gsPort}"));
                var status = $"Status: 127.0.0.1:{_serverListPort} -> {_serverHost}:{_serverListPort} | GS 127.0.0.1:{gsPort} -> {_serverHost}:{gsPort} (auto)";
                if (InvokeRequired) BeginInvoke(() => lblStatus.Text = status);
                else lblStatus.Text = status;
            };

            try
            {
                _proxy.Start();
                btnStart.Text = "Stop Proxy";
                lblStatus.Text = $"Status: 127.0.0.1:{_serverListPort} -> {_serverHost}:{_serverListPort} | GS: aguardando C116 (auto)";
                Log(new LogEvent(DateTime.Now, $"[PROXY] Iniciado: ServerList=127.0.0.1:{_serverListPort}, GS=auto (C116)"));
            }
            catch (Exception ex)
            {
                Log(new LogEvent(DateTime.Now, $"[PROXY] Falha ao iniciar: {ex.Message}"));
                _proxy = null;
            }
        }

        // ============ Launch ============

        private void btnBrowse_Click(object? sender, EventArgs e)
        {
            var dlg = new OpenFileDialog { Filter = "main.exe|main.exe", Title = "Selecione o main.exe do cliente MU" };
            if (dlg.ShowDialog() == DialogResult.OK)
            {
                _exePath = dlg.FileName;
                txtExePath.Text = _exePath;
                Log(new LogEvent(DateTime.Now, $"[PATH] main.exe selecionado: {_exePath}"));
            }
        }

        private bool _launching = false;

        private void btnLaunch_Click(object? sender, EventArgs e)
        {
            if (_launching) return;
            _launching = true;
            btnLaunch.Enabled = false;
            try
            {
                string exePath = txtExePath.Text.Trim();
                if (string.IsNullOrWhiteSpace(exePath)) exePath = _exePath;
                _exePath = exePath;
                string exeDir = Path.GetDirectoryName(exePath)!;

                if (!File.Exists(exePath))
                { Log(new LogEvent(DateTime.Now, $"[LAUNCH] main.exe não encontrado: {exePath}")); return; }

                foreach (var p in Process.GetProcessesByName("main"))
                { try { p.Kill(); } catch { } }

                var si = new STARTUPINFO { cb = Marshal.SizeOf<STARTUPINFO>() };
                if (!CreateProcess(exePath, $"\"{exePath}\"", IntPtr.Zero, IntPtr.Zero, false,
                    0, IntPtr.Zero, exeDir, ref si, out var pi))
                {
                    Log(new LogEvent(DateTime.Now, $"[LAUNCH] CreateProcess falhou: {Marshal.GetLastWin32Error()}"));
                    return;
                }
                Log(new LogEvent(DateTime.Now, $"[LAUNCH] main.exe iniciado — PID={pi.dwProcessId}"));
                CloseHandle(pi.hThread);
                CloseHandle(pi.hProcess);
            }
            finally
            {
                _launching = false;
                btnLaunch.Enabled = true;
            }
        }

        // ============ Misc UI ============

        private void btnClear_Click(object sender, EventArgs e) => txtLog.Clear();
        private void btnClearPacket_Click(object sender, EventArgs e) => lvPackets.Items.Clear();

        private string BytesToAscii(byte[] data)
        {
            var sb = new StringBuilder(data.Length);
            foreach (var b in data) sb.Append(b >= 0x20 && b <= 0x7E ? (char)b : '.');
            return sb.ToString();
        }

        private void lvPackets_DoubleClick(object sender, EventArgs e)
        {
            if (lvPackets.SelectedItems.Count == 0) return;
            txtPacket.Text = lvPackets.SelectedItems[0].SubItems[4].Text;
        }

        private void lvPackets_KeyDown(object? sender, KeyEventArgs e)
        {
            if (e.KeyCode == Keys.C && e.Control)
            { e.SuppressKeyPress = true; CopySelectedPacketsToClipboard(); }
        }

        private void CopySelectedPacketsToClipboard()
        {
            if (lvPackets.SelectedItems.Count == 0) return;
            var sb = new StringBuilder();
            foreach (ListViewItem item in lvPackets.SelectedItems)
            {
                for (int i = 0; i < item.SubItems.Count; i++)
                { if (i > 0) sb.Append('\t'); sb.Append(item.SubItems[i].Text); }
                sb.AppendLine();
            }
            try { Clipboard.SetText(sb.ToString()); } catch { }
        }

        private void rbClient_CheckedChanged(object sender, EventArgs e) => rbServer.Checked = false;
        private void rbServer_CheckedChanged(object sender, EventArgs e) => rbClient.Checked = false;

        private void btnSendPacketDecoded_Click(object sender, EventArgs e)
        {
            try
            {
                var clean = txtPacket.Text.Replace(" ", "").Replace("-", "");
                var data = Convert.FromHexString(clean);
                var dir = rbServer.Checked ? "Server" : "Client";
                // Usa o protocolo tipado SEND_PACKET (pipe separator)
                int sent = _dll?.SendCommand($"SEND_PACKET|{dir}|{clean}") ?? 0;
                if (sent > 0)
                    Log(new LogEvent(DateTime.Now, $"[SEND] →byp.dll [{dir}] {txtPacket.Text} (len={data.Length})"));
                else
                    Log(new LogEvent(DateTime.Now, "[SEND] ERRO: byp.dll nao conectada"));
            }
            catch (Exception ex)
            { Log(new LogEvent(DateTime.Now, $"[SEND] Hex inválido: {ex.Message}")); }
        }

        private void btnBypassPkt_Click(object sender, EventArgs e) => bypassed.Add(txtPacket.Text);
        private void cbC4_CheckedChanged(object sender, EventArgs e) { }
    }
}
