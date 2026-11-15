namespace WinFormsMitm.Protocol.Packets
{
    // CGMainCheckRecv (C1:0F)
    [Packet(0x0F, 0x00)]
    public class MainCheckPacket : IPacket
    {
        public PacketId Id => new PacketId(0x0F, 0x00);
    }
}
