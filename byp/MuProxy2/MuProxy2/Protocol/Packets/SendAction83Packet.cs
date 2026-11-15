namespace WinFormsMitm.Protocol.Packets
{
    [Packet(PacketType.SendAction83, 0x83, 0x00)]
    public class SendAction83Packet : IPacket
    {
        public PacketId Id => new PacketId(0x83, 0x00);
    }
}