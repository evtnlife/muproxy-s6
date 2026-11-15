using System;
using System.Threading;
using System.Threading.Tasks;

namespace WinFormsMitm.Protocol
{
    /// <summary>
    /// Controla o ataque automático enviando comandos CMD para o byp.dll via TCP.
    /// 
    /// Protocolo CMD suportado pelo byp.dll:
    ///   CMD ATTACK START &lt;targetHex&gt; [delay=&lt;ms&gt;] [range=&lt;float&gt;] [skill=&lt;hex&gt;] [type=&lt;0|1&gt;]
    ///   CMD ATTACK STOP
    ///   CMD ATTACK ONCE  &lt;targetHex&gt; [skill=&lt;hex&gt;] [type=&lt;0|1&gt;]
    ///   CMD ATTACK CONFIG [delay=&lt;ms&gt;] [range=&lt;float&gt;] [skill=&lt;hex&gt;] [type=&lt;0|1&gt;]
    ///   CMD MONSTER LIST  [filter=&lt;mask&gt;] [range=&lt;float&gt;]
    ///   CMD MONSTER START [interval=&lt;ms&gt;] [filter=&lt;mask&gt;] [range=&lt;float&gt;]
    ///   CMD MONSTER STOP
    ///   CMD MONSTER CONFIG [interval=&lt;ms&gt;] [filter=&lt;mask&gt;] [range=&lt;float&gt;]
    /// </summary>
    public sealed class MonsterAttacker : IDisposable
    {
        private readonly Action<string>? _sendCommand;
        private readonly Action<string>? _log;

        // Configuração de ataque
        public int AttackRange { get; set; } = 8;
        public int Action { get; set; } = 0x0A;
        public int Direction { get; set; } = 0xCF;
        private bool _enabled = false;
        public bool Enabled
        {
            get => _enabled;
            set { if (value != _enabled) SetEnabled(value); }
        }
        public bool AttackAllMonsters { get; set; } = false;
        public TimeSpan AttackInterval { get; set; } = TimeSpan.FromMilliseconds(800);

        // Configuração do tipo de ataque (0=normal, 1=skill)
        public int AttackType { get; set; } = 0;
        // Skill ID para ataques do tipo skill
        public int SkillId { get; set; } = 0;

        // Jitter aleatório +/- 15% para ficar menos robótico
        public double IntervalJitterPercent { get; set; } = 0.15;

        private readonly object _lock = new();
        private readonly Random _rng = new();

        /// <summary>
        /// Cria o atacador enviando comandos CMD para o byp.dll.
        /// </summary>
        public MonsterAttacker(Action<string>? sendCommand, Action<string>? log = null)
        {
            _sendCommand = sendCommand;
            _log = log;
        }

        /// <summary>
        /// Habilita/desabilita o ataque automático.
        /// Ao habilitar, envia CMD ATTACK START (a byp.dll ataca todos no range).
        /// Ao desabilitar, envia CMD ATTACK STOP.
        /// </summary>
        public bool SetEnabled(bool enabled)
        {
            if (_sendCommand == null) return false;

            if (enabled)
            {
                int delayMs = (int)AttackInterval.TotalMilliseconds;
                string cmd = $"CMD ATTACK START delay={delayMs} range={AttackRange} type={AttackType}";
                if (SkillId != 0)
                    cmd += $" skill={SkillId:X4}";
                SendCommand(cmd);
                _log?.Invoke($"[ATK] START auto-attack -> {cmd} (SkillId=0x{SkillId:X4})");
                _enabled = true;
            }
            else
            {
                SendCommand("CMD ATTACK STOP");
                _log?.Invoke("[ATK] STOP auto-attack");
                _enabled = false;
            }
            return true;
        }

        /// <summary>
        /// Ataque individual não é mais usado: a byp.dll ataca todos os monstros
        /// no range via sua própria thread. Mantido para compatibilidade.
        /// </summary>
        public bool AttackTarget(int entityIndex)
        {
            // Delegado: simplesmente garante que o ataque está ligado.
            return SetEnabled(true);
        }

        /// <summary>Envia CMD ATTACK CONFIG para atualizar defaults no byp.dll.</summary>
        public void SendConfig()
        {
            if (_sendCommand == null) return;

            int delayMs = (int)AttackInterval.TotalMilliseconds;
            string cmd = $"CMD ATTACK CONFIG delay={delayMs} range={AttackRange} type={AttackType}";
            if (SkillId != 0)
                cmd += $" skill={SkillId:X4}";

            _log?.Invoke($"[ATK] CONFIG -> {cmd} (SkillId=0x{SkillId:X4})");
            SendCommand(cmd);
        }

        /// <summary>Envia CMD MONSTER LIST para solicitar lista de objetos.</summary>
        public void RequestMonsterList(int filterMask = 7, float range = 0)
        {
            string cmd = $"CMD MONSTER LIST filter={filterMask}";
            if (range > 0)
                cmd += $" range={range}";
            SendCommand(cmd);
        }

        /// <summary>Envia CMD MONSTER START para scan periódico.</summary>
        public void StartMonsterScan(int intervalMs = 500, int filterMask = 7, float range = 0)
        {
            string cmd = $"CMD MONSTER START interval={intervalMs} filter={filterMask}";
            if (range > 0)
                cmd += $" range={range}";
            SendCommand(cmd);
        }

        /// <summary>Envia CMD MONSTER STOP.</summary>
        public void StopMonsterScan()
        {
            SendCommand("CMD MONSTER STOP");
        }

        /// <summary>Envia CMD ATTACK DUMP — despeja ObjectManager completo com WORDs detalhados.</summary>
        public void RequestDump()
        {
            SendCommand("CMD ATTACK DUMP");
            _log?.Invoke("[ATK] DUMP requested");
        }

        /// <summary>Envia CMD ATTACK HEXDUMP slot=N — hex dump de um slot específico.</summary>
        public void RequestHexDump(int slotIndex)
        {
            SendCommand($"CMD ATTACK HEXDUMP slot={slotIndex}");
            _log?.Invoke($"[ATK] HEXDUMP slot={slotIndex} requested");
        }

        private void SendCommand(string command)
        {
            if (_sendCommand == null)
            {
                _log?.Invoke("[ATK] ERRO: sendCommand delegate é null");
                return;
            }
            _sendCommand(command);
        }

        // Compatibilidade: AttackAllInRange agora é o próprio ataque automático.
        public int AttackAllInRange()
        {
            return SetEnabled(true) ? 1 : 0;
        }

        // Compatibilidade: AttackIndex delega para AttackTarget.
        public bool AttackIndex(int index, int x = 0, int y = 0)
        {
            return AttackTarget(index);
        }

        // Compatibilidade: AttackNearest liga o ataque automático.
        public bool AttackNearest()
        {
            return SetEnabled(true);
        }

        public void Dispose()
        {
            if (_enabled) SetEnabled(false);
        }
    }
}
