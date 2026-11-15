namespace WinFormsMitm.Protocol.Packets
{
    [Packet(PacketType.ServerInfo, 0x3B, 0x00)]
    public class ServerInfoPacket : IPacket
    {
        public PacketId Id => new PacketId(0x3B, 0x00);
    }
}