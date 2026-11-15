namespace WinFormsMitm.Protocol.Packets
{
    [Packet(PacketType.MapServerMove, 0xB1, 0x00)]
    public class MapServerMovePacket : IPacket
    {
        public PacketId Id => new PacketId(0xB1, 0x00);
    }
}