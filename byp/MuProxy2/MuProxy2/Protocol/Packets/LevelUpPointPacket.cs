namespace WinFormsMitm.Protocol.Packets
{
    [Packet(PacketType.LevelUpPoint, 0xF3, 0x06)]
    public class LevelUpPointPacket : IPacket
    {
        public PacketId Id => new PacketId(0xF3, 0x06);
    }
}