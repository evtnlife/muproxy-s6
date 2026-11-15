namespace WinFormsMitm.Protocol.Packets
{
    [Packet(PacketType.LevelUp, 0xF3, 0x05)]
    public class LevelUpPacket : IPacket
    {
        public PacketId Id => new PacketId(0xF3, 0x05);
    }
}