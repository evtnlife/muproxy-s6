namespace WinFormsMitm.Protocol.Packets
{
    [Packet(PacketType.MasterLevelUp, 0xF3, 0x51)]
    public class MasterLevelUpPacket : IPacket
    {
        public PacketId Id => new PacketId(0xF3, 0x51);
    }
}