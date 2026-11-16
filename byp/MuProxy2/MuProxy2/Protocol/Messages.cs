using System.Globalization;

namespace WinFormsMitm.Protocol
{
    // ================================================================
    // Mensagens C++ → C# (DLL envia para o Proxy)
    // ================================================================

    /// <summary>Um slot do ObjectManager (monstro, NPC, player).</summary>
    [MessageTag("ENTITY")]
    public class EntityMessage
    {
        public int Slot, CompositeIndex, Type, Level;
        public float X, Y, Z, Distance;
        public string Name = "";

        public static EntityMessage Parse(string payload)
        {
            var p = payload.Split('|');
            return new EntityMessage
            {
                Slot           = int.Parse(p[0]),
                CompositeIndex = int.Parse(p[1]),
                Type           = int.Parse(p[2]),
                Level          = int.Parse(p[3]),
                X              = float.Parse(p[4], CultureInfo.InvariantCulture),
                Y              = float.Parse(p[5], CultureInfo.InvariantCulture),
                Z              = float.Parse(p[6], CultureInfo.InvariantCulture),
                Distance       = float.Parse(p[7], CultureInfo.InvariantCulture),
                Name           = p.Length > 8 ? p[8] : ""
            };
        }
    }

    /// <summary>Fim do lote de entidades. Dispara OnMonsterList.</summary>
    [MessageTag("ENTITY_BATCH_END")]
    public class EntityBatchEnd
    {
        public int Count;

        public static EntityBatchEnd Parse(string payload)
        {
            return new EntityBatchEnd { Count = int.Parse(payload) };
        }
    }

    /// <summary>Linha de log vinda da DLL.</summary>
    [MessageTag("LOG")]
    public class LogEntryMessage
    {
        public string Text = "";

        public static LogEntryMessage Parse(string payload)
        {
            return new LogEntryMessage { Text = payload };
        }
    }

    /// <summary>Pacote decriptado vindo do byp.dll (PacketSniffer). Formato: PKT|SEND|hex ou PKT|RECV|hex</summary>
    [MessageTag("PKT")]
    public class PktMessage
    {
        public string Direction = "";  // "SEND" ou "RECV"
        public byte[] Data = Array.Empty<byte>();

        public static PktMessage Parse(string payload)
        {
            int sep = payload.IndexOf('|');
            if (sep < 0) return new PktMessage { Direction = payload, Data = Array.Empty<byte>() };
            string dir = payload.Substring(0, sep);
            string hex = payload.Substring(sep + 1).Replace(" ", "");
            return new PktMessage
            {
                Direction = dir,
                Data = hex.Length > 0 ? Convert.FromHexString(hex) : Array.Empty<byte>()
            };
        }
    }

    // ================================================================
    // Mensagens C# → C++ (Proxy envia para a DLL)
    // ================================================================

    /// <summary>Inicia ataque automático.</summary>
    [MessageTag("ATTACK_START")]
    public class AttackStartMessage
    {
        public int DelayMs, Range, HitCount, AttackType, SkillId;

        public static AttackStartMessage Parse(string payload) => new(); // não recebido, só enviado

        public string ToWire()
        {
            string s = $"ATTACK_START|delay={DelayMs}|range={Range}|type={AttackType}|hitcount={HitCount}";
            if (SkillId != 0) s += $"|skill={SkillId:X4}";
            return s;
        }
    }

    /// <summary>Para ataque automático.</summary>
    [MessageTag("ATTACK_STOP")]
    public class AttackStopMessage
    {
        public static AttackStopMessage Parse(string payload) => new();
        public string ToWire() => "ATTACK_STOP|";
    }

    /// <summary>Atualiza config de ataque sem parar.</summary>
    [MessageTag("ATTACK_CONFIG")]
    public class AttackConfigMessage
    {
        public int DelayMs, Range, HitCount, AttackType, SkillId;

        public static AttackConfigMessage Parse(string payload) => new();
        public string ToWire()
        {
            string s = $"ATTACK_CONFIG|delay={DelayMs}|range={Range}|type={AttackType}|hitcount={HitCount}";
            if (SkillId != 0) s += $"|skill={SkillId:X4}";
            return s;
        }
    }

    /// <summary>Seta o composite index do jogador local.</summary>
    [MessageTag("ATTACK_PLAYERIDX")]
    public class AttackPlayerIdxMessage
    {
        public int PlayerIndex;

        public static AttackPlayerIdxMessage Parse(string payload) => new();
        public string ToWire() => $"ATTACK_PLAYERIDX|idx={PlayerIndex:X4}";
    }

    /// <summary>Dump de diagnóstico do ObjectManager.</summary>
    [MessageTag("ATTACK_DUMP")]
    public class AttackDumpMessage
    {
        public static AttackDumpMessage Parse(string payload) => new();
        public string ToWire() => "ATTACK_DUMP|";
    }

    /// <summary>Inicia tracker de monstros.</summary>
    [MessageTag("MONSTER_START")]
    public class MonsterStartMessage
    {
        public int Interval, Filter;

        public static MonsterStartMessage Parse(string payload) => new();
        public string ToWire() => $"MONSTER_START|interval={Interval}|filter={Filter}";
    }

    /// <summary>Para tracker de monstros.</summary>
    [MessageTag("MONSTER_STOP")]
    public class MonsterStopMessage
    {
        public static MonsterStopMessage Parse(string payload) => new();
        public string ToWire() => "MONSTER_STOP|";
    }

    /// <summary>Injeta pacote no client ou server.</summary>
    [MessageTag("SEND_PACKET")]
    public class SendPacketMessage
    {
        public string Direction = "";  // "Client" ou "Server"
        public byte[] Data = Array.Empty<byte>();

        public static SendPacketMessage Parse(string payload) => new();
        public string ToWire() => $"SEND_PACKET|{Direction}|{Convert.ToHexString(Data).Replace("-", " ")}";
    }

    // ================================================================
    // Auto potion (byp.dll — módulo AutoPotion)
    // ================================================================

    /// <summary>Liga o auto-potion (bebe HP/MP abaixo do limiar).</summary>
    [MessageTag("POTION_START")]
    public class PotionStartMessage
    {
        public static PotionStartMessage Parse(string payload) => new();
        public string ToWire() => "POTION_START|";
    }

    /// <summary>Desliga o auto-potion.</summary>
    [MessageTag("POTION_STOP")]
    public class PotionStopMessage
    {
        public static PotionStopMessage Parse(string payload) => new();
        public string ToWire() => "POTION_STOP|";
    }

    /// <summary>
    /// Configura o auto-potion.
    /// HpThreshold/MpThreshold = % que dispara a potion; IntervalMs = intervalo
    /// mínimo entre usos (anti-hack do servidor); HpSlot/MpSlot = slot do
    /// inventário (-1 = usa o modelo aprendido pelo próprio jogo).
    /// </summary>
    [MessageTag("POTION_CONFIG")]
    public class PotionConfigMessage
    {
        public int HpThreshold = 50;
        public int MpThreshold = 30;
        public int IntervalMs = 1000;
        public int HpSlot = -1;
        public int MpSlot = -1;

        public static PotionConfigMessage Parse(string payload) => new();
        public string ToWire()
        {
            string s = $"POTION_CONFIG|hp={HpThreshold}|mp={MpThreshold}|interval={IntervalMs}";
            if (HpSlot >= 0) s += $"|hpslot={HpSlot}";
            if (MpSlot >= 0) s += $"|mpslot={MpSlot}";
            return s;
        }
    }

    /// <summary>Pede o status do auto-potion (a resposta sai no log do byp.dll).</summary>
    [MessageTag("POTION_STATUS")]
    public class PotionStatusMessage
    {
        public static PotionStatusMessage Parse(string payload) => new();
        public string ToWire() => "POTION_STATUS|";
    }
}
