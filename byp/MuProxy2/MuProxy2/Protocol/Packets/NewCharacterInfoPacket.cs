namespace WinFormsMitm.Protocol.Packets
{
    [Packet(PacketType.NewCharacterInfo, 0xF3, 0xE0)]
    public class NewCharacterInfoPacket : IPacket
    {
        public PacketId Id => new PacketId(0xF3, 0xE0);
    }
}