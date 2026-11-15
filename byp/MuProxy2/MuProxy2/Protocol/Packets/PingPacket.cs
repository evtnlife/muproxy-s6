namespace WinFormsMitm.Protocol.Packets
{
    [Packet(PacketType.Ping, 0xF4, 0x06)]
    public class PingPacket : IPacket
    {
        public PacketId Id => new PacketId(0xF4, 0x06);
    }
}