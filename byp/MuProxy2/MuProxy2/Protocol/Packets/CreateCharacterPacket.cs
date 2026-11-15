namespace WinFormsMitm.Protocol.Packets
{
    [Packet(PacketType.CreateCharacter, 0x31, 0x00)]
    public class CreateCharacterPacket : IPacket
    {
        public PacketId Id => new PacketId(0x31, 0x00);
    }
}