using System;
using System.Collections.Generic;

namespace WinFormsMitm.Protocol
{
    public class PacketDefinition
    {
        public PacketType Type { get; set; }
        public string Name { get; set; } = "";
        public PacketId Id { get; set; }
        public List<PacketField> Fields { get; set; } = new();
        public Type? ClrType { get; set; }
    }
}
