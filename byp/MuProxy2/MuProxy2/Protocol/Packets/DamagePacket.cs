namespace WinFormsMitm.Protocol.Packets
{
    // struct PMSG_DAMAGE_RECV (C1:11)
    //   header      [0..2]  C1 08 11
    //   index       [3..4]  target index
    //   damage      [5..6]  damage value
    //   type        [7]     flags (bit 0x10 = crit, bit 0x20 = outro)
    //   ShieldDamage[8..9]
    //   ViewCurHP   [10..13]
    //   ViewCurSD   [14..17]
    //   ViewDamageHP[18..21]
    //   ViewDamageSD[22..25]
    [Packet(PacketType.Damage, 0x11, 0x00)]
    public class DamagePacket : IPacket
    {
        public PacketId Id => new PacketId(0x11, 0x00);

        [PacketField(3, Size = 2)]
        public ushort TargetIndex { get; set; }

        [PacketField(5, Size = 2)]
        public ushort Damage { get; set; }

        // Campo 'type' (offset 7) desmembrado em bits
        [PacketField(7, BitStart = 0, BitCount = 4)]
        public byte TypeBase { get; set; }

        [PacketField(7, BitStart = 4, BitCount = 1)]
        public bool TypeCritical { get; set; }   // bit 0x10

        [PacketField(7, BitStart = 5, BitCount = 1)]
        public bool TypeStrong { get; set; }     // bit 0x20

        [PacketField(7, BitStart = 6, BitCount = 2)]
        public byte TypeReserved { get; set; }

        [PacketField(8, Size = 2)]
        public ushort ShieldDamage { get; set; }

        [PacketField(10, Size = 4)]
        public uint ViewCurHP { get; set; }

        [PacketField(14, Size = 4)]
        public uint ViewCurSD { get; set; }

        [PacketField(18, Size = 4)]
        public uint ViewDamageHP { get; set; }

        [PacketField(22, Size = 4)]
        public uint ViewDamageSD { get; set; }
    }
}