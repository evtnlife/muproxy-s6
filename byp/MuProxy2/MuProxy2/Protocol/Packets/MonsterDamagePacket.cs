namespace WinFormsMitm.Protocol.Packets
{
    [Packet(PacketType.MonsterDamage, 0xF3, 0x07)]
    public class MonsterDamagePacket : IPacket
    {
        public PacketId Id => new PacketId(0xF3, 0x07);
    }
}