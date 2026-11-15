namespace WinFormsMitm.Protocol
{
    public readonly struct PacketId
    {
        public byte Opcode { get; }
        public byte SubCode { get; }

        public PacketId(byte opcode, byte subCode)
        {
            Opcode = opcode;
            SubCode = subCode;
        }

        public ushort Value => (ushort)((Opcode << 8) | SubCode);

        public override string ToString() => $"0x{Opcode:X2}-0x{SubCode:X2}";
    }
}
