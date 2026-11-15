using System;
using System.Linq;
using System.Reflection;

namespace WinFormsMitm.Protocol
{
    /// <summary>
    /// Serializa uma instância de IPacket para byte[].
    /// Usa os atributos [Packet] (para header/opcode) e [PacketField] (para campos).
    /// </summary>
    public static class PacketSerializer
    {
        /// <summary>
        /// Serializa um IPacket em bytes prontos para envio.
        /// O tamanho do header (C1/C2/C3/C4) e campos são determinados pelos atributos.
        /// </summary>
        public static byte[] Serialize(IPacket packet)
        {
            if (packet == null) throw new ArgumentNullException(nameof(packet));

            var type = packet.GetType();
            var pktAttr = type.GetCustomAttribute<PacketAttribute>()
                ?? throw new InvalidOperationException($"Tipo {type.Name} não tem [Packet] attribute.");

            var fields = type.GetProperties()
                .Select(p => (Prop: p, Attr: p.GetCustomAttribute<PacketFieldAttribute>()))
                .Where(x => x.Attr != null)
                .OrderBy(x => x.Attr!.Offset)
                .ToList();

            // Determina tamanho do pacote: campos vão até o maior offset+size
            int maxEnd = 0;
            foreach (var f in fields)
            {
                int end = f.Attr!.Offset + f.Attr.Size;
                if (end > maxEnd) maxEnd = end;
            }

            // Header C1 = 2 bytes, opcode = 1 byte (ou 2 se F3/F4), campos = maxEnd - header - opcode
            byte headerByte = 0xC1; // padrão: pacote curto não-criptografado
            int headerSize = 2;
            int opcodeSize = pktAttr.Id.SubCode != 0 ? 2 : 1;
            int totalLen = headerSize + opcodeSize + (maxEnd > 0 ? maxEnd - headerSize - opcodeSize : 0);

            // Se totalLen > 255, precisa C2/C4
            if (totalLen > 255)
            {
                headerByte = 0xC2;
                headerSize = 3;
                totalLen = headerSize + opcodeSize + (maxEnd > 0 ? maxEnd - 3 : 0);
            }

            // Recalcula com base no maior offset+size dos campos
            totalLen = maxEnd > 0 ? maxEnd : headerSize + opcodeSize;

            byte[] data = new byte[totalLen];

            // Header
            data[0] = headerByte;
            if (headerByte == 0xC1 || headerByte == 0xC3)
                data[1] = (byte)totalLen;
            else
            {
                data[1] = (byte)((totalLen >> 8) & 0xFF);
                data[2] = (byte)(totalLen & 0xFF);
            }

            // Opcode
            data[headerSize] = pktAttr.Id.Opcode;
            if (opcodeSize == 2)
                data[headerSize + 1] = pktAttr.Id.SubCode;

            // Campos
            foreach (var f in fields)
            {
                var prop = f.Prop;
                var attr = f.Attr!;
                int offset = attr.Offset;
                int size = attr.Size;

                switch (attr.Kind)
                {
                    case FieldKind.UInt8 when prop.PropertyType == typeof(byte):
                        data[offset] = (byte)prop.GetValue(packet)!;
                        break;

                    case FieldKind.UInt16 when prop.PropertyType == typeof(ushort):
                    {
                        // MU protocol: big-endian (high byte first, como SET_NUMBERHB/LB)
                        ushort v = (ushort)prop.GetValue(packet)!;
                        data[offset] = (byte)((v >> 8) & 0xFF);
                        data[offset + 1] = (byte)(v & 0xFF);
                        break;
                    }

                    case FieldKind.UInt16 when prop.PropertyType == typeof(short):
                    {
                        // MU protocol: big-endian (high byte first)
                        short v = (short)prop.GetValue(packet)!;
                        data[offset] = (byte)((v >> 8) & 0xFF);
                        data[offset + 1] = (byte)(v & 0xFF);
                        break;
                    }

                    case FieldKind.UInt32 when prop.PropertyType == typeof(uint):
                    {
                        // MU protocol: big-endian (high byte first, como SET_NUMBERHW/LW e SET_NUMBERHDW/LDW)
                        uint v = (uint)prop.GetValue(packet)!;
                        data[offset] = (byte)((v >> 24) & 0xFF);
                        data[offset + 1] = (byte)((v >> 16) & 0xFF);
                        data[offset + 2] = (byte)((v >> 8) & 0xFF);
                        data[offset + 3] = (byte)(v & 0xFF);
                        break;
                    }

                    case FieldKind.UInt32 when prop.PropertyType == typeof(int):
                    {
                        // MU protocol: big-endian (high byte first)
                        int v = (int)prop.GetValue(packet)!;
                        data[offset] = (byte)((v >> 24) & 0xFF);
                        data[offset + 1] = (byte)((v >> 16) & 0xFF);
                        data[offset + 2] = (byte)((v >> 8) & 0xFF);
                        data[offset + 3] = (byte)(v & 0xFF);
                        break;
                    }

                    case FieldKind.EntityIndex when prop.PropertyType == typeof(ushort):
                    {
                        ushort v = (ushort)prop.GetValue(packet)!;
                        data[offset] = (byte)(v & 0xFF);
                        data[offset + 1] = (byte)((v >> 8) & 0xFF);
                        break;
                    }

                    case FieldKind.AsciiString when prop.PropertyType == typeof(string):
                    {
                        string s = (string)prop.GetValue(packet)!;
                        for (int i = 0; i < size && i < s.Length; i++)
                            data[offset + i] = (byte)s[i];
                        break;
                    }

                    case FieldKind.Bytes when prop.PropertyType == typeof(byte[]):
                    {
                        byte[] arr = (byte[])prop.GetValue(packet)!;
                        int len = Math.Min(size == 9999 ? arr.Length : size, data.Length - offset);
                        Array.Copy(arr, 0, data, offset, len);
                        break;
                    }

                    case FieldKind.Bit:
                    case FieldKind.BitFlag:
                    {
                        // Bits são escritos no byte existente
                        if (prop.PropertyType == typeof(bool))
                        {
                            bool val = (bool)prop.GetValue(packet)!;
                            if (val)
                                data[attr.Offset] |= (byte)(1 << attr.BitStart);
                            else
                                data[attr.Offset] &= (byte)~(1 << attr.BitStart);
                        }
                        else if (prop.PropertyType == typeof(int) || prop.PropertyType == typeof(byte))
                        {
                            int val = Convert.ToInt32(prop.GetValue(packet));
                            for (int b = 0; b < attr.BitCount; b++)
                            {
                                if ((val & (1 << b)) != 0)
                                    data[attr.Offset] |= (byte)(1 << (attr.BitStart + b));
                                else
                                    data[attr.Offset] &= (byte)~(1 << (attr.BitStart + b));
                            }
                        }
                        break;
                    }

                    default:
                        // UInt8 fallback
                        if (prop.PropertyType == typeof(byte) && size == 1)
                        {
                            data[offset] = (byte)prop.GetValue(packet)!;
                        }
                        else if (prop.PropertyType == typeof(ushort) && size == 2)
                        {
                            ushort v = (ushort)prop.GetValue(packet)!;
                            data[offset] = (byte)(v & 0xFF);
                            data[offset + 1] = (byte)((v >> 8) & 0xFF);
                        }
                        else
                        {
                            throw new NotSupportedException(
                                $"Campo {prop.Name} tipo={prop.PropertyType.Name} kind={attr.Kind} não suportado na serialização.");
                        }
                        break;
                }
            }

            return data;
        }

        /// <summary>
        /// Serializa um IPacket para string hex (formato sem hífen: "C10711ED540FCD").
        /// </summary>
        public static string ToHex(IPacket packet)
        {
            return BitConverter.ToString(Serialize(packet)).Replace("-", "");
        }
    }
}