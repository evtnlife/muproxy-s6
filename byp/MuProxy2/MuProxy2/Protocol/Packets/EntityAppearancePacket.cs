namespace WinFormsMitm.Protocol.Packets
{
    [Packet(PacketType.EntityAppearance, 0x7E, 0x00)]
    public class EntityAppearancePacket : IPacket
    {
        public PacketId Id => new PacketId(0x7E, 0x00);
    }
}