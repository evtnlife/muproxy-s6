namespace WinFormsMitm.Protocol.Packets
{
    [Packet(PacketType.SendAction10, 0x10, 0x00)]
    public class SendAction10Packet : IPacket
    {
        public PacketId Id => new PacketId(0x10, 0x00);
    }
}