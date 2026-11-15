using System.Globalization;
using System.Text;
using System.Text.RegularExpressions;

namespace WinFormsMitm.Tests;

public static class PacketLogParser
{
    private static readonly Regex LineRegex = new(
        @"^\[(?<ts>\d{4}-\d{2}-\d{2}\s+\d{2}:\d{2}:\d{2}\.\d{3})\]\s+\[(?<tag>[A-Z]+)\]\s+\[(?<dir>[^\]]+)\]\s+len=(?<len>\d+)\s+hex=(?<hex>[0-9A-Fa-f]+)$",
        RegexOptions.Compiled);

    public static List<PacketLogEntry> Parse(string path)
    {
        var entries = new List<PacketLogEntry>();
        foreach (var line in File.ReadLines(path))
        {
            var m = LineRegex.Match(line.Trim());
            if (!m.Success) continue;

            var hex = m.Groups["hex"].Value;
            if (hex.Length % 2 != 0) continue;

            var raw = GameServerCrypto.FromHex(hex);

            // Ignora trafego do canal de controle LOG (comeca com 'L' = 0x4C ASCII)
            if (raw.Length > 0 && raw[0] == 0x4C)
                continue;

            var decrypted = GameServerCrypto.Decrypt(raw);
            var header = decrypted.Length > 0 ? decrypted[0] : (byte)0;
            var category = header switch
            {
                0xC1 => "C1",
                0xC2 => "C2",
                0xC3 => "C3",
                0xC4 => "C4",
                _ => "OTHER"
            };

            entries.Add(new PacketLogEntry
            {
                Timestamp = DateTime.ParseExact(m.Groups["ts"].Value, "yyyy-MM-dd HH:mm:ss.fff", CultureInfo.InvariantCulture),
                Tag = m.Groups["tag"].Value,
                Dir = m.Groups["dir"].Value,
                Length = int.Parse(m.Groups["len"].Value),
                Hex = hex,
                Raw = raw,
                Decrypted = decrypted,
                HeaderCategory = category
            });
        }
        return entries;
    }
}
