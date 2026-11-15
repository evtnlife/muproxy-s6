namespace WinFormsMitm.Protocol.Packets
{
    /// <summary>
    /// RECV 0xD4 — Move (monstro/entidade/jogador).
    /// Sub-códigos variam: 0x02 movimento de monstro, 0x32/0x33 posição do jogador.
    /// </summary>
    [Packet(PacketType.EntityMove, 0xD4, 0x00)]
    public class MovePacket : IPacket
    {
        public PacketId Id => new PacketId(0xD4, 0x00);

        [PacketField(3, Size = 1)]
        public byte SubCode { get; set; }

        [PacketField(4, Size = 2)]
        public ushort IndexOrX { get; set; }

        [PacketField(6, Size = 1)]
        public byte X { get; set; }

        [PacketField(7, Size = 1)]
        public byte Y { get; set; }
    }
}

