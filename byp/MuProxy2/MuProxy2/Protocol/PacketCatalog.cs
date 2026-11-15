using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Reflection;

namespace WinFormsMitm.Protocol
{
    public static class PacketCatalog
    {
        private static readonly Dictionary<PacketId, List<PacketDefinition>> _byId = new();
        private static readonly List<PacketDefinition> _definitions = new();

        /// <summary>
        /// Deriva o FieldKind a partir do tipo da propriedade (bool -> Bit, byte -> UInt8,
        /// ushort -> UInt16, uint -> UInt32, string -> AsciiString, etc).
        /// </summary>
        private static FieldKind DeriveKind(Type propType, int size)
        {
            if (propType == typeof(bool)) return FieldKind.Bit;
            if (propType == typeof(byte)) return FieldKind.UInt8;
            if (propType == typeof(ushort)) return FieldKind.UInt16;
            if (propType == typeof(short)) return FieldKind.Int16;
            if (propType == typeof(uint)) return FieldKind.UInt32;
            if (propType == typeof(int)) return FieldKind.Int32;
            if (propType == typeof(string)) return FieldKind.AsciiString;
            if (propType == typeof(byte[])) return FieldKind.Bytes;
            // default
            return size switch
            {
                1 => FieldKind.UInt8,
                2 => FieldKind.UInt16,
                4 => FieldKind.UInt32,
                _ => FieldKind.Bytes
            };
        }

        static PacketCatalog()
        {
            var packetTypes = typeof(PacketCatalog).Assembly.GetTypes()
                .Where(t => t.GetCustomAttribute<PacketAttribute>() != null);

            foreach (var type in packetTypes)
            {
                var attr = type.GetCustomAttribute<PacketAttribute>()!;
                var def = new PacketDefinition
                {
                    Type = attr.Type,
                    Name = attr.Type == PacketType.Other ? type.Name.Replace("Packet", "") : attr.Type.ToString(),
                    Id = attr.Id,
                    ClrType = type,
                    Fields = type.GetProperties()
                        .SelectMany(p => p.GetCustomAttributes<PacketFieldAttribute>()
                            .Select(fa => new PacketField
                            {
                                Name = p.Name,
                                Kind = fa.Kind == FieldKind.UInt8 ? DeriveKind(p.PropertyType, fa.Size) : fa.Kind,
                                Offset = fa.Offset,
                                Length = fa.Size,
                                ByteOffset = fa.Offset,
                                BitStart = fa.BitStart,
                                BitCount = fa.BitCount,
                                Mask = fa.Mask
                            }))
                        .OrderBy(f => f.Offset)
                        .ToList()
                };

                _definitions.Add(def);
                if (!_byId.ContainsKey(attr.Id))
                    _byId[attr.Id] = new List<PacketDefinition>();
                _byId[attr.Id].Add(def);
            }

            Debug.WriteLine($"[PacketCatalog] Registered {_definitions.Count} packet definitions.");
            foreach (var def in _definitions)
                Debug.WriteLine($"[PacketCatalog]   {def.Id} => {def.Name} ({def.Type})");
        }

        public static PacketDefinition Identify(byte[] data, string direction)
        {
            if (data.Length < 3) return Unknown();

            int headerSize = data[0] switch
            {
                0xC1 or 0xC3 => 2,
                0xC2 or 0xC4 => 3,
                _ => 2
            };

            int opPos = headerSize;
            if (data.Length <= opPos) return Unknown();

            byte b1 = data[opPos];
            // Only 0xF3 and 0xF4 use 2-byte opcodes; everything else is 1-byte
            bool isTwoByte = (b1 == 0xF3 || b1 == 0xF4) && data.Length > opPos + 1;
            var id = new PacketId(b1, isTwoByte ? data[opPos + 1] : (byte)0);

            if (_byId.TryGetValue(id, out var defs))
            {
                // Resolve ambiguidades 0x11: RECV dano (DamagePacket) vs SEND ataque (AttackMonsterPacket).
                if (id.Opcode == 0x11)
                {
                    bool isSend = direction.Contains("SEND") || direction.Contains("C->S");
                    var match = defs.Find(d => d.Name == (isSend ? "AttackMonster" : "Damage"));
                    if (match != null) return match;
                }
                return defs[0];
            }

            return Unknown();
        }

        public static PacketDefinition Unknown() => new PacketDefinition { Type = PacketType.Unknown, Name = "Unknown" };

        public static IEnumerable<PacketType> KnownTypes() => _definitions
            .Select(d => d.Type).Distinct().OrderBy(t => t.ToString());

        public static IReadOnlyList<PacketDefinition> Definitions => _definitions;
    }
}
