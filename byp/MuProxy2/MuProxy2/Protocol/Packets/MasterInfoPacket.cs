namespace WinFormsMitm.Protocol.Packets
{
    [Packet(PacketType.MasterInfo, 0xF3, 0x50)]
    public class MasterInfoPacket : IPacket
    {
        public PacketId Id => new PacketId(0xF3, 0x50);
    }
}