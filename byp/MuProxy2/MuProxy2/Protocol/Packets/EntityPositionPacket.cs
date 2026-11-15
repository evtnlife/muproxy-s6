namespace WinFormsMitm.Protocol.Packets
{
    /// <summary>
    /// RECV 0x22 — posição de entidade.
    /// C1 NN 22 [idxHi] [idxLo] [x] [y] [dir?]
    /// (MU protocol: high byte first)
    /// </summary>
    [Packet(PacketType.EntityPosition, 0x22, 0x00)]
    public class EntityPositionPacket : IPacket
    {
        public PacketId Id => new PacketId(0x22, 0x00);

        [PacketField(3, Size = 2)]
        public ushort Index { get; set; }

        [PacketField(5, Size = 1)]
        public byte X { get; set; }

        [PacketField(6, Size = 1)]
        public byte Y { get; set; }

        [PacketField(7, Size = 1)]
        public byte Direction { get; set; }
    }
}