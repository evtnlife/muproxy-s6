namespace WinFormsMitm.Protocol.Packets
{
    /// <summary>
    /// Skill List Multi-entity (servidor -> cliente). Formato: C2 00 66 F3 E2 [data].
    /// Envia skills de múltiplas entidades.
    /// </summary>
    [Packet(PacketType.SkillListMulti, 0xF3, 0xE2)]
    public class SkillListMultiPacket : IPacket
    {
        public PacketId Id => new PacketId(0xF3, 0xE2);

        [PacketField(5, Size = 97)]
        public byte[] Data { get; set; } = Array.Empty<byte>();
    }
}
