namespace WinFormsMitm.Protocol.Packets
{
    [Packet(PacketType.WeatherTime, 0xFF, 0x00)]
    public class WeatherTimePacket : IPacket
    {
        public PacketId Id => new PacketId(0xFF, 0x00);
    }
}