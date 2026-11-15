namespace WinFormsMitm.Protocol.Packets
{
    /// <summary>
    /// Ataque físico (cliente -> servidor) opcode PROTOCOL_CODE2 (0x11 para LANGUAGE==1).
    /// Formato: C1 07 11 [targetHi] [targetLo] [action] [dir]
    /// 
    /// O targetIndex NÃO é o viewport entity index — é um índice composto:
    ///   targetIndex = (viewport_idxLo << 8) | monsterClass_lo
    /// Onde viewport_idxLo é o byte baixo do índice de viewport (sem flags)
    /// e monsterClass_lo é o byte baixo da classe do monstro (sem flags).
    /// 
    /// Serialização big-endian (high byte first, como SET_NUMBERHB/LB no MuEmu).
    /// Exemplo: monstro viewport=02-ED, class=00-18 -> target=0xED18 -> C1-07-11-ED-18-0A-CF
    /// </summary>
    [Packet(PacketType.AttackMonster, 0x11, 0x00)]
    public class AttackMonsterPacket : IPacket
    {
        public PacketId Id => new PacketId(0x11, 0x00);

        [PacketField(3, Size = 2, Kind = FieldKind.UInt16)]
        public ushort TargetIndex { get; set; }

        [PacketField(5, Size = 1)]
        public byte Action { get; set; }

        [PacketField(6, Size = 1)]
        public byte Direction { get; set; }
    }
}