namespace WinFormsMitm.Protocol.Packets
{
    [Packet(PacketType.ServerAck, 0x3A, 0x00)]
    public class ServerAckPacket : IPacket
    {
        public PacketId Id => new PacketId(0x3A, 0x00);
    }
}