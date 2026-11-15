namespace WinFormsMitm.Protocol.Packets
{
    [Packet(PacketType.CloseClient, 0xF1, 0x02)]
    public class CloseClientPacket : IPacket
    {
        public PacketId Id => new PacketId(0xF1, 0x02);
    }
}