namespace WinFormsMitm.Protocol.Packets
{
    [Packet(PacketType.CharacterInfo, 0xF3, 0x03)]
    public class CharacterInfoPacket : IPacket
    {
        public PacketId Id => new PacketId(0xF3, 0x03);
    }
}