namespace WinFormsMitm.Protocol
{
    public enum FieldKind
    {
        UInt8,
        UInt16,
        UInt32,
        Int16,
        Int32,
        Bytes,
        AsciiString,
        EntityIndex,
        CoordsXY,
        Bit,
        BitFlag,
        Skip
    }

    public class PacketField
    {
        public string Name { get; set; } = "";
        public FieldKind Kind { get; set; }
        public int Offset { get; set; }
        public int Length { get; set; } // for Bytes / AsciiString, or skip count
        public string? Format { get; set; }

        // For Bit fields
        public int ByteOffset { get; set; }
        public int BitStart { get; set; }
        public int BitCount { get; set; }

        // For BitFlag: the mask
        public byte Mask { get; set; }
    }
}
