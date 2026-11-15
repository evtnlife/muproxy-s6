namespace WinFormsMitm.Protocol.Packets
{
    [Packet(PacketType.QuestReward, 0xA3, 0x00)]
    public class QuestRewardPacket : IPacket
    {
        public PacketId Id => new PacketId(0xA3, 0x00);
    }
}