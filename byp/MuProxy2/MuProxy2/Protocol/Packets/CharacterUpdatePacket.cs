namespace WinFormsMitm.Protocol.Packets
{
    [Packet(PacketType.CharacterUpdate, 0x36, 0x00)]
    public class CharacterUpdatePacket : IPacket
    {
        public PacketId Id => new PacketId(0x36, 0x00);
    }
}