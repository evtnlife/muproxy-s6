namespace WinFormsMitm.Protocol.Packets
{
    [Packet(PacketType.EntityRangeCheck, 0x28, 0x00)]
    public class EntityRangeCheckPacket : IPacket
    {
        public PacketId Id => new PacketId(0x28, 0x00);
    }
}