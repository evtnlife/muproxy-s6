using WinFormsMitm.Protocol;

namespace WinFormsMitm.Services
{
    /// <summary>
    /// Serviço de controle de ataque automático.
    /// Gerencia configuração e envia mensagens tipadas para o byp.dll.
    /// NÃO tem loop/timer — apenas envia comandos quando o estado muda.
    /// </summary>
    public class AttackService
    {
        private readonly DllCommandService _dll;
        private readonly Action<string>? _log;

        // ---- Configuração ----
        public int Range { get; set; } = 8;
        public int DelayMs { get; set; } = 800;
        public int HitCount { get; set; } = 1;     // quantas vezes ataca cada alvo por ciclo
        public int AttackType { get; set; } = 0;   // 0=normal, 1=skill
        public int SkillId { get; set; } = 0;
        public bool AttackMonsters { get; set; } = true;       // atacar monstros (type 2)
        public bool AttackPlayersAll { get; set; } = false;    // atacar todos os jogadores (type 1)
        public bool AttackPlayersClick { get; set; } = false;  // atacar jogador no clique (até sumir)
        public bool AttackOthers { get; set; } = false;        // atacar outros (não-monstro, não-jogador)

        private bool _enabled;
        public bool Enabled
        {
            get => _enabled;
            set
            {
                if (value == _enabled) return;
                _enabled = value;
                OnStateChanged?.Invoke(value);
            }
        }

        // ---- Eventos para UI ----
        public event Action<bool>? OnStateChanged;

        public AttackService(DllCommandService dll, Action<string>? log = null)
        {
            _dll = dll;
            _log = log;
        }

        public void Start()
        {
            var msg = new AttackStartMessage
            {
                DelayMs = DelayMs,
                Range = Range,
                AttackType = AttackType,
                HitCount = HitCount,
                SkillId = SkillId,
                AttackMonsters = AttackMonsters ? 1 : 0,
                AttackPlayersAll = AttackPlayersAll ? 1 : 0,
                AttackPlayersClick = AttackPlayersClick ? 1 : 0,
                AttackOthers = AttackOthers ? 1 : 0
            };
            string wire = msg.ToWire();

            int sent = _dll.SendCommand(wire);
            if (sent > 0)
            {
                _enabled = true;
                OnStateChanged?.Invoke(true);
                _log?.Invoke($"[ATK] START -> {wire}");
            }
            else
            {
                _log?.Invoke("[ATK] ERRO: byp.dll não conectada");
            }
        }

        public void Stop()
        {
            var msg = new AttackStopMessage();
            _dll.SendCommand(msg.ToWire());
            _enabled = false;
            OnStateChanged?.Invoke(false);
            _log?.Invoke("[ATK] STOP");
        }

        public void SendConfig()
        {
            var msg = new AttackConfigMessage
            {
                DelayMs = DelayMs,
                Range = Range,
                AttackType = AttackType,
                HitCount = HitCount,
                SkillId = SkillId,
                AttackMonsters = AttackMonsters ? 1 : 0,
                AttackPlayersAll = AttackPlayersAll ? 1 : 0,
                AttackPlayersClick = AttackPlayersClick ? 1 : 0,
                AttackOthers = AttackOthers ? 1 : 0
            };
            _dll.SendCommand(msg.ToWire());
            _log?.Invoke($"[ATK] CONFIG -> {msg.ToWire()}");
        }

        public void SetSkill(int skillId)
        {
            SkillId = skillId;
            AttackType = 1;
            SendConfig();
        }

        public void SetNormalAttack()
        {
            SkillId = 0;
            AttackType = 0;
            SendConfig();
        }
    }
}
