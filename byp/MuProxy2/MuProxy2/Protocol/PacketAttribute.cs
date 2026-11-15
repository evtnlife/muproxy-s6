using System;

namespace WinFormsMitm.Protocol
{
    [AttributeUsage(AttributeTargets.Class, Inherited = false, AllowMultiple = false)]
    public sealed class PacketAttribute : Attribute
    {
        public PacketType Type { get; }
        public PacketId Id { get; }

        // Conveniencia: [Packet(0xF3, 0x15)] usa PacketType.Other
        public PacketAttribute(byte group, byte code)
            : this(PacketType.Other, group, code) { }

        public PacketAttribute(PacketType type, byte group, byte code)
        {
            Type = type;
            Id = new PacketId(group, code);
        }
    }
}
