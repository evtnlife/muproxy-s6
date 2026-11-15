namespace WinFormsMitm.Protocol.Packets
{
    [Packet(PacketType.Mana, 0x27, 0x00)]
    public class ManaPacket : IPacket
    {
        public PacketId Id => new PacketId(0x27, 0x00);
    }
}