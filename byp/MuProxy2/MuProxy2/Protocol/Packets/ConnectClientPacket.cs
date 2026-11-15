namespace WinFormsMitm.Protocol.Packets
{
    [Packet(PacketType.ConnectClient, 0xF1, 0x00)]
    public class ConnectClientPacket : IPacket
    {
        public PacketId Id => new PacketId(0xF1, 0x00);
    }
}