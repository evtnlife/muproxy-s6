namespace WinFormsMitm.Protocol.Packets
{
    [Packet(PacketType.EntityInfo, 0x66, 0x00)]
    public class EntityInfoPacket : IPacket
    {
        public PacketId Id => new PacketId(0x66, 0x00);
    }
}