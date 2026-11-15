namespace WinFormsMitm.Protocol.Packets
{
    [Packet(PacketType.RewardExperience, 0x9C, 0x00)]
    public class RewardExperiencePacket : IPacket
    {
        public PacketId Id => new PacketId(0x9C, 0x00);
    }
}