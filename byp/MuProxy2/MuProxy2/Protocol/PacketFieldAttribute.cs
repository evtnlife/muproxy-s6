using System;

namespace WinFormsMitm.Protocol
{
    [AttributeUsage(AttributeTargets.Property, Inherited = false, AllowMultiple = false)]
    public sealed class PacketFieldAttribute : Attribute
    {
        public int Offset { get; set; }
        public int Size { get; set; }
        public FieldKind Kind { get; set; }

        // For Bit / BitFlag fields
        public int BitStart { get; set; }
        public int BitCount { get; set; }
        public byte Mask { get; set; }

        public PacketFieldAttribute(int offset)
        {
            Offset = offset;
            Size = 1;
            Kind = FieldKind.UInt8;
        }
    }
}
