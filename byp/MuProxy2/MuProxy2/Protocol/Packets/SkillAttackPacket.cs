namespace WinFormsMitm.Protocol.Packets
{
    /// <summary>
    /// Skill Attack (cliente -> servidor). Formato: C3 0D [13 bytes encrypted].
    /// O pacote é cifrado com XOR+sub (0x36/0x07), sem XorFilter.
    /// </summary>
    [Packet(PacketType.SkillAttack, 0xAB, 0x00)]
    public class SkillAttackPacket : IPacket
    {
        public PacketId Id => new PacketId(0xAB, 0x00);

        [PacketField(3, Size = 10)]
        public byte[] Data { get; set; } = Array.Empty<byte>();
    }
}
