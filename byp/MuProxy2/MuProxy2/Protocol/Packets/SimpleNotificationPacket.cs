namespace WinFormsMitm.Protocol.Packets
{
    [Packet(PacketType.SimpleNotification, 0xDF, 0x00)]
    public class SimpleNotificationPacket : IPacket
    {
        public PacketId Id => new PacketId(0xDF, 0x00);
    }
}