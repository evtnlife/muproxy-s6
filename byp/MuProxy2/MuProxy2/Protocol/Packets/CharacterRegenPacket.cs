namespace WinFormsMitm.Protocol.Packets
{
    [Packet(PacketType.CharacterRegen, 0xF3, 0x04)]
    public class CharacterRegenPacket : IPacket
    {
        public PacketId Id => new PacketId(0xF3, 0x04);
    }
}