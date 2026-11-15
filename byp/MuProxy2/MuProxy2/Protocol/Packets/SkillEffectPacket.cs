namespace WinFormsMitm.Protocol.Packets
{
    /// <summary>
    /// Skill Effect (servidor -> cliente). Formato: C3 0D [13 bytes encrypted].
    /// </summary>
    [Packet(PacketType.SkillEffect, 0xDE, 0x00)]
    public class SkillEffectPacket : IPacket
    {
        public PacketId Id => new PacketId(0xDE, 0x00);

        [PacketField(3, Size = 10)]
        public byte[] Data { get; set; } = Array.Empty<byte>();
    }
}
