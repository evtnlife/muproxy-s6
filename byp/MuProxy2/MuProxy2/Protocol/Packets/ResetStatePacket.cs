namespace WinFormsMitm.Protocol.Packets
{
    [Packet(PacketType.ResetState, 0x3D, 0x00)]
    public class ResetStatePacket : IPacket
    {
        public PacketId Id => new PacketId(0x3D, 0x00);
    }
}