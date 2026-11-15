using System.Text;
using Xunit;
using Xunit.Abstractions;

namespace WinFormsMitm.Tests;

/// <summary>
/// Teste que aplica chaves por header descobertas no brute-force e avalia
/// se o pacote decifrado como um todo faz sentido.
/// </summary>
public class PerHeaderKeyTests
{
    private readonly ITestOutputHelper _output;

    public PerHeaderKeyTests(ITestOutputHelper output)
    {
        _output = output;
    }

    // Mapeamento header original -> (xor, sub, addFirst) descoberto por brute-force
    private static readonly Dictionary<byte, (byte xor, byte sub, bool addFirst)> HeaderKeys = new()
    {
        [0x01] = (0x00, 0x3D, false), // (c ^ 0x00) - 0x3D -> C4
        [0x02] = (0x00, 0x3E, false), // (c ^ 0x00) - 0x3E -> C4
        [0x04] = (0x00, 0x40, false), // (c ^ 0x00) - 0x40 -> C4
    };

    [Fact]
    public void Apply_Per_Header_Keys_And_Generate_Report()
    {
        var logPath = @"F:\blackrockmu\logs_backup_2026-08-05\byp_packets.log";
        var entries = PacketLogParser.Parse(logPath);

        var report = new StringBuilder();
        report.AppendLine($"# Relatorio de Decifracao por Header - {DateTime.Now:yyyy-MM-dd HH:mm:ss}");
        report.AppendLine();

        var stats = new Dictionary<string, int>();
        var muHeaders = new HashSet<byte> { 0xC1, 0xC2, 0xC3, 0xC4 };

        foreach (var entry in entries)
        {
            if (entry.Raw.Length == 0) continue;
            var header = entry.Raw[0];

            byte[] result;
            string method;
            if (muHeaders.Contains(header))
            {
                // Ja esta decifrado (ou e pacote de controle ASCII)
                result = entry.Raw;
                method = "plain";
            }
            else if (HeaderKeys.TryGetValue(header, out var keys))
            {
                result = CryptoBrute.Transform(entry.Raw, keys.xor, keys.sub, keys.addFirst);
                method = $"header_0x{header:X2}_xor_0x{keys.xor:X2}_sub_0x{keys.sub:X2}";
            }
            else
            {
                result = entry.Raw;
                method = $"unknown_0x{header:X2}";
            }

            var cat = result.Length > 0 && muHeaders.Contains(result[0]) ? $"C{result[0] - 0xC1 + 1}" : "OTHER";
            stats[cat] = stats.GetValueOrDefault(cat) + 1;

            // Loga exemplos (primeiros 5 de cada categoria)
            if (stats[cat] <= 5)
            {
                report.AppendLine($"[{entry.Timestamp:HH:mm:ss.fff}] [{entry.Dir}] method={method} len={entry.Raw.Length}");
                report.AppendLine($"  raw: {GameServerCrypto.ToHex(entry.Raw)}");
                report.AppendLine($"  out: {GameServerCrypto.ToHex(result)}");
                report.AppendLine($"  printable_ratio: {PrintableRatio(result):F2}");
                report.AppendLine();
            }
        }

        report.AppendLine("Estatisticas finais:");
        foreach (var kv in stats.OrderByDescending(x => x.Value))
            report.AppendLine($"  {kv.Key}: {kv.Value}");

        var reportPath = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "per_header_report.txt");
        File.WriteAllText(reportPath, report.ToString());
        _output.WriteLine(report.ToString());
        _output.WriteLine($"Relatorio salvo em: {reportPath}");
    }

    private static double PrintableRatio(byte[] data)
    {
        if (data.Length == 0) return 0;
        int printable = data.Count(b => b is >= 0x20 and <= 0x7E or 0x00 or 0x0A or 0x0D);
        return (double)printable / data.Length;
    }
}
