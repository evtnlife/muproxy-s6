namespace WinFormsMitm.Protocol.Packets
{
    [Packet(PacketType.Heartbeat, 0xC6, 0x00)]
    public class HeartbeatPacket : IPacket
    {
        public PacketId Id => new PacketId(0xC6, 0x00);
    }
}