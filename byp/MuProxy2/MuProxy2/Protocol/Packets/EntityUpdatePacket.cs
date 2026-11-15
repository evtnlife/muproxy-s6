namespace WinFormsMitm.Protocol.Packets
{
    /// <summary>
    /// RECV 0x14 — lista de entidades na viewport / update.
    /// C1 NN 14 [count] [idxLo idxHi]...
    /// </summary>
    [Packet(PacketType.EntityUpdate, 0x14, 0x00)]
    public class EntityUpdatePacket : IPacket
    {
        public PacketId Id => new PacketId(0x14, 0x00);

        [PacketField(3, Size = 1)]
        public byte Count { get; set; }

        public byte[] RawIndexes { get; set; } = Array.Empty<byte>();
    }
}