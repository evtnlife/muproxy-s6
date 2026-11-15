namespace WinFormsMitm.Protocol.Packets
{
    [Packet(PacketType.EntityAlloc, 0x2D, 0x00)]
    public class EntityAllocPacket : IPacket
    {
        public PacketId Id => new PacketId(0x2D, 0x00);
    }
}