namespace WinFormsMitm.Protocol.Packets
{
    // CGOptionData (C1:F3:30)
    [Packet(0xF3, 0x30)]
    public class OptionDataPacket : IPacket
    {
        public PacketId Id => new PacketId(0xF3, 0x30);

        [PacketField(0)]
        public byte Result { get; set; }
    }
}
