namespace WinFormsMitm.Protocol.Packets
{
    /// <summary>
    /// Skill Attack Large (cliente -> servidor). Formato: C3 18 [24 bytes encrypted].
    /// </summary>
    [Packet(PacketType.SkillAttackLarge, 0xCC, 0x00)]
    public class SkillAttackLargePacket : IPacket
    {
        public PacketId Id => new PacketId(0xCC, 0x00);

        [PacketField(3, Size = 21)]
        public byte[] Data { get; set; } = Array.Empty<byte>();
    }
}
