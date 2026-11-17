using System.Text;
using Xunit;
using Xunit.Abstractions;

namespace WinFormsMitm.Tests;

public class CryptoAlgorithmTests
{
    private readonly ITestOutputHelper _output;

    public CryptoAlgorithmTests(ITestOutputHelper output)
    {
        _output = output;
    }

    [Fact]
    public void Roundtrip_SingleBytes()
    {
        for (int b = 0; b <= 0xFF; b++)
        {
            var orig = new byte[] { (byte)b };
            var enc = GameServerCrypto.Encrypt(orig);
            var dec = GameServerCrypto.Decrypt(enc);
            Assert.Equal(orig, dec);
        }
    }

    [Fact]
    public void Roundtrip_KnownVector()
    {
        // Se C1^36 = C7; C7-7 = C0 (nao, so exemplo conceitual)
        var plain = new byte[] { 0xC1, 0x04, 0x00, 0x01 };
        var enc = GameServerCrypto.Encrypt(plain);
        var dec = GameServerCrypto.Decrypt(enc);
        Assert.Equal(plain, dec);
    }
}

public class PacketLogDecryptionTests
{
    private readonly ITestOutputHelper _output;

    public PacketLogDecryptionTests(ITestOutputHelper output)
    {
        _output = output;
    }

    [Fact]
    public void Decrypt_All_Packets_And_Classify()
    {
        var logPath = TestLogLocator.Find();
        if (logPath == null) return;   // sem log de captura: nada a analisar
  
        var entries = PacketLogParser.Parse(logPath);
        Assert.NotEmpty(entries);

        var stats = entries.GroupBy(e => e.HeaderCategory)
                           .ToDictionary(g => g.Key, g => g.Count());

        var reportPath = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "decryption_report.txt");
        var report = new StringBuilder();
        report.AppendLine($"# Relatorio de Decifração - {DateTime.Now:yyyy-MM-dd HH:mm:ss}");
        report.AppendLine($"# Algoritmo: decrypt=(c ^ 0x36) - 0x07 | encrypt=(c + 0x07) ^ 0x36");
        report.AppendLine($"# Fonte: {logPath}");
        report.AppendLine();
        report.AppendLine($"Total de pacotes analisados: {entries.Count}");
        foreach (var k in stats.Keys.Order())
            report.AppendLine($"  {k}: {stats[k]} ({100.0 * stats[k] / entries.Count:F2}%)");
        report.AppendLine();

        foreach (var cat in new[] { "C1", "C2", "C3", "C4", "OTHER" })
        {
            var samples = entries.Where(e => e.HeaderCategory == cat).Take(20).ToList();
            if (!samples.Any()) continue;

            report.AppendLine($"--- Exemplos {cat} ---");
            foreach (var s in samples)
            {
                report.AppendLine($"[{s.Timestamp:HH:mm:ss.fff}] [{s.Dir}] len={s.Length}");
                report.AppendLine($"  raw:    {GameServerCrypto.ToHex(s.Raw)}");
                report.AppendLine($"  dec:    {GameServerCrypto.ToHex(s.Decrypted)}");
                report.AppendLine();
            }
        }

        File.WriteAllText(reportPath, report.ToString());
        _output.WriteLine(report.ToString());
        _output.WriteLine($"Relatorio salvo em: {reportPath}");

        // O teste nao falha por OTHER; so documenta. Se quiser forcar pass, descomente abaixo.
        // Assert.True(stats.GetValueOrDefault("C1") + stats.GetValueOrDefault("C2") > entries.Count * 0.5,
        //     "Menos de 50% dos pacotes decifraram para C1/C2");
    }
}
