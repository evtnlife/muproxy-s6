namespace WinFormsMitm.Protocol.Packets
{
    [Packet(PacketType.MapState, 0x07, 0x00)]
    public class MapStatePacket : IPacket
    {
        public PacketId Id => new PacketId(0x07, 0x00);
    }
}