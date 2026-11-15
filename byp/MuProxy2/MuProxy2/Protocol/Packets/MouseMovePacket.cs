namespace WinFormsMitm.Protocol.Packets
{
    [Packet(PacketType.MouseMove, 0xD0, 0x00)]
    public class MouseMovePacket : IPacket
    {
        public PacketId Id => new PacketId(0xD0, 0x00);
    }
}