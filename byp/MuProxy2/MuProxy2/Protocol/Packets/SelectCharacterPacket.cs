namespace WinFormsMitm.Protocol.Packets
{
    // C1:0F:F3:15 + 10 bytes name
    [Packet(0xF3, 0x15)]
    public class SelectCharacterPacket : IPacket
    {
        public PacketId Id => new PacketId(0xF3, 0x15);

        [PacketField(0, Size = 10)]
        public string Name { get; set; } = string.Empty;
    }
}