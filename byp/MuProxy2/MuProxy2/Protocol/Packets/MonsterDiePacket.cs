namespace WinFormsMitm.Protocol.Packets
{
    [Packet(PacketType.MonsterDie, 0x16, 0x00)]
    public class MonsterDiePacket : IPacket
    {
        public PacketId Id => new PacketId(0x16, 0x00);
    }
}