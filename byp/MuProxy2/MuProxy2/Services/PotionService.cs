using WinFormsMitm.Protocol;

namespace WinFormsMitm.Services
{
    /// <summary>
    /// Serviço de controle do auto-potion (bebe HP/MP automaticamente).
    /// Envia comandos tipados para o byp.dll (módulo AutoPotion).
    /// O status do módulo volta pelo log do byp.dll (tag POTION).
    /// </summary>
    public class PotionService
    {
        private readonly DllCommandService _dll;
        private readonly Action<string>? _log;

        // ---- Configuração ----
        public int HpThreshold { get; set; } = 50;   // % de life que dispara a potion
        public int MpThreshold { get; set; } = 30;   // % de mana que dispara a potion
        public int IntervalMs { get; set; } = 1000;  // intervalo mínimo entre usos (anti-hack)
        public int HpSlot { get; set; } = -1;        // slot do inventário (-1 = modelo aprendido)
        public int MpSlot { get; set; } = -1;
        public int HpItemId { get; set; } = -1;      // ID do item (hex) — o byp resolve o slot
        public int MpItemId { get; set; } = -1;      // pela lista de inventário (F3 10)

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

        public PotionService(DllCommandService dll, Action<string>? log = null)
        {
            _dll = dll;
            _log = log;
        }

        public void Start()
        {
            // Só liga/desliga. A configuração é enviada pelo botão "Aplicar"
            // (POTION_APPLY) — mandar a config junto fazia o módulo ficar
            // meio-morto (STOP -> CONFIG -> START) a cada clique.
            var msg = new PotionStartMessage();
            string wire = msg.ToWire();

            int sent = _dll.SendCommand(wire);
            if (sent > 0)
            {
                _enabled = true;
                OnStateChanged?.Invoke(true);
                _log?.Invoke($"[POTION] START -> {wire}");
            }
            else
            {
                _log?.Invoke("[POTION] ERRO: byp.dll não conectada");
            }
        }

        public void Stop()
        {
            var msg = new PotionStopMessage();
            _dll.SendCommand(msg.ToWire());
            _enabled = false;
            OnStateChanged?.Invoke(false);
            _log?.Invoke("[POTION] STOP");
        }

        /// <summary>
        /// Botão "Aplicar": manda UMA mensagem que aplica a configuração e
        /// arma o módulo (idempotente). Também serve para "religar" o
        /// auto-potion sem precisar mexer no checkbox.
        /// </summary>
        public void Apply()
        {
            var msg = new PotionApplyMessage
            {
                HpThreshold = HpThreshold,
                MpThreshold = MpThreshold,
                IntervalMs = IntervalMs,
                HpSlot = HpSlot,
                MpSlot = MpSlot,
                HpItemId = HpItemId,
                MpItemId = MpItemId
            };
            string wire = msg.ToWire();
            int sent = _dll.SendCommand(wire);
            if (sent > 0)
            {
                _enabled = true;
                OnStateChanged?.Invoke(true);
                _log?.Invoke($"[POTION] APPLY -> {wire}");
            }
            else
            {
                _log?.Invoke("[POTION] ERRO: byp.dll não conectada");
            }
        }

        public void SendConfig()
        {
            var msg = new PotionConfigMessage
            {
                HpThreshold = HpThreshold,
                MpThreshold = MpThreshold,
                IntervalMs = IntervalMs,
                HpSlot = HpSlot,
                MpSlot = MpSlot,
                HpItemId = HpItemId,
                MpItemId = MpItemId
            };
            string wire = msg.ToWire();
            _dll.SendCommand(wire);
            _log?.Invoke($"[POTION] CONFIG -> {wire}");
        }

        /// <summary>Pede o estado do módulo (resposta no log do byp.dll).</summary>
        public void RequestStatus()
        {
            var msg = new PotionStatusMessage();
            _dll.SendCommand(msg.ToWire());
            _log?.Invoke("[POTION] STATUS solicitado (resposta no log)");
        }
    }
}
