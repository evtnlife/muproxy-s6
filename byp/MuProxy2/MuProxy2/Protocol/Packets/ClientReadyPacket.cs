namespace WinFormsMitm.Protocol.Packets
{
    [Packet(PacketType.ClientReady, 0xF4, 0x03)]
    public class ClientReadyPacket : IPacket
    {
        public PacketId Id => new PacketId(0xF4, 0x03);
    }
}