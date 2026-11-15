namespace WinFormsMitm.Protocol.Packets
{
    [Packet(PacketType.ObjectList, 0x62, 0x00)]
    public class ObjectListPacket : IPacket
    {
        public PacketId Id => new PacketId(0x62, 0x00);
    }
}