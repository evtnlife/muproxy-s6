namespace WinFormsMitm.Protocol.Packets
{
    [Packet(PacketType.GensBattleInfo, 0xF3, 0xE3)]
    public class GensBattleInfoPacket : IPacket
    {
        public PacketId Id => new PacketId(0xF3, 0xE3);
    }
}