namespace WinFormsMitm.Protocol.Packets
{
    [Packet(PacketType.UserDie, 0x17, 0x00)]
    public class UserDiePacket : IPacket
    {
        public PacketId Id => new PacketId(0x17, 0x00);
    }
}