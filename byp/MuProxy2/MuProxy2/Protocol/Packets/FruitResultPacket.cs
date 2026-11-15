namespace WinFormsMitm.Protocol.Packets
{
    [Packet(PacketType.FruitResult, 0x2C, 0x00)]
    public class FruitResultPacket : IPacket
    {
        public PacketId Id => new PacketId(0x2C, 0x00);
    }
}