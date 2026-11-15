namespace WinFormsMitm.Protocol.Packets
{
    [Packet(PacketType.StringMessage, 0x78, 0x00)]
    public class StringMessagePacket : IPacket
    {
        public PacketId Id => new PacketId(0x78, 0x00);
    }
}