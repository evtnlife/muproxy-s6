namespace WinFormsMitm.Protocol.Packets
{
    [Packet(PacketType.NewCharacterCalc, 0xF3, 0xE1)]
    public class NewCharacterCalcPacket : IPacket
    {
        public PacketId Id => new PacketId(0xF3, 0xE1);
    }
}