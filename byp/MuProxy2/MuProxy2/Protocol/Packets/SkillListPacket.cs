namespace WinFormsMitm.Protocol.Packets
{
    /// <summary>
    /// Skill List (servidor -> cliente). Formato: C1 5C F3 E5 [data].
    /// Envia a lista de skills do personagem.
    /// </summary>
    [Packet(PacketType.SkillList, 0xF3, 0xE5)]
    public class SkillListPacket : IPacket
    {
        public PacketId Id => new PacketId(0xF3, 0xE5);

        [PacketField(4, Size = 88)]
        public byte[] Data { get; set; } = Array.Empty<byte>();
    }
}
