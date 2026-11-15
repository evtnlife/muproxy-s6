namespace WinFormsMitm.Protocol.Packets
{
    [Packet(PacketType.CharacterList, 0xF3, 0x00)]
    public class CharacterListPacket : IPacket
    {
        public PacketId Id => new PacketId(0xF3, 0x00);
    }
}