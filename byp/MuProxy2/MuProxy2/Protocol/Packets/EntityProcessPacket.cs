namespace WinFormsMitm.Protocol.Packets
{
    [Packet(PacketType.EntityProcess, 0x2E, 0x00)]
    public class EntityProcessPacket : IPacket
    {
        public PacketId Id => new PacketId(0x2E, 0x00);
    }
}