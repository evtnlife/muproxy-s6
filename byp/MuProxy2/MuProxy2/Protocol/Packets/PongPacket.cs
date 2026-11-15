namespace WinFormsMitm.Protocol.Packets
{
    [Packet(PacketType.Pong, 0xF4, 0xF4)]
    public class PongPacket : IPacket
    {
        public PacketId Id => new PacketId(0xF4, 0xF4);
    }
}