namespace WinFormsMitm.Protocol.Packets
{
    // CGCharacterNameCheck (C1:F3:15) - client checks name availability
    [Packet(0xF3, 0x15)]
    public class CharacterNameCheckPacket : IPacket
    {
        public PacketId Id => new PacketId(0xF3, 0x15);

        [PacketField(0, Size = 10)]
        public string Name { get; set; } = string.Empty;
    }
}
