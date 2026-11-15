namespace WinFormsMitm.Protocol.Packets
{
    /// <summary>
    /// OBSOLETO: WalkToPacket foi substituído por MovePacket (opcode 0xD4) 
    /// e AttackMonsterPacket (opcode 0x11). Mantido apenas para compatibilidade
    /// de compilação; opcode 0xFE nunca será matcheado.
    /// </summary>
    [Packet(PacketType.WalkTo, 0xFE, 0x00)]
    public class WalkToPacket : IPacket
    {
        public PacketId Id => new PacketId(0xFE, 0x00);

        [PacketField(3, Size = 2)]
        public ushort TargetIndex { get; set; }

        [PacketField(5, Size = 1)]
        public byte Action { get; set; }

        [PacketField(6, Size = 1)]
        public byte Direction { get; set; }
    }
}