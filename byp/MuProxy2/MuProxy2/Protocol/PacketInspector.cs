using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace WinFormsMitm.Protocol
{
    public static class PacketInspector
    {
        public static string Describe(byte[] data, string direction)
        {
            var def = PacketCatalog.Identify(data, direction);
            if (def.Type == PacketType.Unknown)
                return "";

            int headerSize = data[0] switch
            {
                0xC1 or 0xC3 => 2,
                0xC2 or 0xC4 => 3,
                _ => 2
            };

            // Two-byte opcodes (0xF3, 0xF4) consume an extra byte
            bool isTwoByte = (data[headerSize] == 0xF3 || data[headerSize] == 0xF4) && data.Length > headerSize + 1;
            int pos = headerSize + (isTwoByte ? 2 : 1);

            if (def.Name.Contains("Ping") || def.Name.Contains("Pong"))
                return def.Name;

            var sb = new StringBuilder();
            sb.Append(def.Name);

            foreach (var field in def.Fields)
            {
                if (pos >= data.Length) break;
                switch (field.Kind)
                {
                    case FieldKind.UInt8:
                        sb.Append($" {field.Name}={data[pos]:X2}");
                        pos++;
                        break;
                    case FieldKind.UInt16:
                        if (pos + 1 < data.Length)
                        {
                            // MU protocol: big-endian (high byte first)
                            ushort v = (ushort)((data[pos] << 8) | data[pos + 1]);
                            sb.Append($" {field.Name}=0x{v:X4}");
                            pos += 2;
                        }
                        break;
                    case FieldKind.UInt32:
                        if (pos + 3 < data.Length)
                        {
                            // MU protocol: big-endian (high byte first)
                            uint v = (uint)((data[pos] << 24) | (data[pos + 1] << 16) | (data[pos + 2] << 8) | data[pos + 3]);
                            sb.Append($" {field.Name}=0x{v:X8}");
                            pos += 4;
                        }
                        break;
                    case FieldKind.EntityIndex:
                        if (pos + 1 < data.Length)
                        {
                            // MU protocol: big-endian, high byte may contain flag bits
                            ushort raw = (ushort)((data[pos] << 8) | data[pos + 1]);
                            ushort idx = (ushort)((data[pos] & 0x3F) << 8 | data[pos + 1]);
                            sb.Append($" {field.Name}=0x{idx:X4}(raw=0x{raw:X4})");
                            pos += 2;
                        }
                        break;
                    case FieldKind.CoordsXY:
                        if (pos + 1 < data.Length)
                        {
                            sb.Append($" {field.Name}=({data[pos]},{data[pos + 1]})");
                            pos += 2;
                        }
                        break;
                    case FieldKind.AsciiString:
                        int len = Math.Min(field.Length, data.Length - pos);
                        var s = new string(Enumerable.Range(0, len)
                            .Select(i => data[pos + i] >= 0x20 && data[pos + i] <= 0x7E ? (char)data[pos + i] : '.')
                            .ToArray());
                        sb.Append($" {field.Name}={s}");
                        pos += len;
                        break;
                    case FieldKind.Bytes:
                        int blen = field.Length == 9999 ? data.Length - pos : Math.Min(field.Length, data.Length - pos);
                        if (blen > 0)
                            sb.Append($" {field.Name}[{blen}]");
                        pos += blen;
                        break;
                    case FieldKind.Bit:
                        if (field.ByteOffset < data.Length)
                        {
                            int bits = field.BitCount <= 0 ? 1 : field.BitCount;
                            int bitPos = field.BitStart;
                            int val = 0;
                            for (int b = 0; b < bits; b++)
                                val = (val << 1) | ((data[field.ByteOffset] >> (bitPos + b)) & 1);
                            sb.Append($" {field.Name}={val}");
                        }
                        break; // nao avanca pos (bits leem o mesmo byte)
                    case FieldKind.BitFlag:
                        if (field.ByteOffset < data.Length)
                        {
                            byte mask = field.Mask;
                            byte val = (byte)(data[field.ByteOffset] & mask);
                            sb.Append($" {field.Name}=0x{val:X2}");
                        }
                        break; // nao avanca pos
                }
            }

            // Heuristic entity list summary for entity-related packets
            if (def.Type == PacketType.EntityUpdate || def.Type == PacketType.EntityPosition ||
                def.Type == PacketType.EntityMove || def.Type == PacketType.ObjectList ||
                def.Type == PacketType.EntitySpawn || def.Type == PacketType.EntityInfo ||
                def.Type == PacketType.EntityAppearance || def.Type == PacketType.Damage ||
                def.Type == PacketType.ObjectAnimation)
            {
                var entities = TryParseEntityList(data, headerSize, def.Name);
                if (entities.Any())
                    sb.Append(" | " + string.Join(", ", entities.Take(5)) + (entities.Count > 5 ? $" ...+{entities.Count - 5}" : ""));
            }

            return sb.ToString();
        }

        private static List<string> TryParseEntityList(byte[] data, int headerSize, string defName)
        {
            var result = new List<string>();
            int pos = headerSize + 1;

            // Try 6-byte struct: (index 2, type 2, x 1, y 1)
            // Fallback to 4-byte: (index 2, x 1, y 1)
            while (pos + 4 <= data.Length)
            {
                ushort idx = (ushort)(data[pos] | (data[pos + 1] << 8));
                if (idx > 0x7FFF) { pos++; continue; }

                // Try to read type (next 2 bytes) — if it looks like a valid mob type (1..0x1FF) or player (0)
                ushort type = 0;
                bool hasType = false;
                if (pos + 6 <= data.Length)
                {
                    type = (ushort)(data[pos + 2] | (data[pos + 3] << 8));
                    // Player types are usually 0 or very low; mob types are 1..0x1FF
                    if (type <= 0x200)
                    {
                        byte x = data[pos + 4];
                        byte y = data[pos + 5];
                        string label = type == 0 ? "Player" : $"Mob=0x{type:X3}";
                        result.Add($"{label}@0x{idx:X4}({x},{y})");
                        pos += 6;
                        hasType = true;
                    }
                }
                if (!hasType)
                {
                    byte x = data[pos + 2];
                    byte y = data[pos + 3];
                    result.Add($"Ent=0x{idx:X4}({x},{y})");
                    pos += 4;
                }
            }
            return result;
        }
    }
}
