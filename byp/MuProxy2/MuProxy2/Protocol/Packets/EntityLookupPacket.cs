namespace WinFormsMitm.Protocol.Packets
{
    [Packet(PacketType.EntityLookup, 0x2A, 0x00)]
    public class EntityLookupPacket : IPacket
    {
        public PacketId Id => new PacketId(0x2A, 0x00);
    }
}