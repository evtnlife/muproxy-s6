namespace WinFormsMitm.Protocol.Packets
{
    [Packet(PacketType.Life, 0x26, 0x00)]
    public class LifePacket : IPacket
    {
        public PacketId Id => new PacketId(0x26, 0x00);
    }
}