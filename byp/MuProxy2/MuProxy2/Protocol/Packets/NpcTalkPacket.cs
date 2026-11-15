namespace WinFormsMitm.Protocol.Packets
{
    // CGNpcTalk (C1:30)
    [Packet(PacketType.NpcTalk, 0x30, 0x00)]
    public class NpcTalkPacket : IPacket
    {
        public PacketId Id => new PacketId(0x30, 0x00);
    }
}
