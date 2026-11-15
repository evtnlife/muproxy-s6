namespace WinFormsMitm.Protocol.Packets
{
    [Packet(PacketType.NewMessage, 0xF3, 0xE4)]
    public class NewMessagePacket : IPacket
    {
        public PacketId Id => new PacketId(0xF3, 0xE4);
    }
}