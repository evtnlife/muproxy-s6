using System.Text;
using Xunit;
using Xunit.Abstractions;

namespace WinFormsMitm.Tests;

public class HeaderPatternAnalysisTests
{
    private readonly ITestOutputHelper _output;

    public HeaderPatternAnalysisTests(ITestOutputHelper output)
    {
        _output = output;
    }

    [Fact]
    public void Analyze_Raw_Headers_And_Possible_Ciphered_Equivalents()
    {
        var logPath = @"F:\blackrockmu\logs_backup_2026-08-05\byp_packets.log";
        var entries = PacketLogParser.Parse(logPath);

        // Conta headers no log original (antes de qualquer transform)
        var originalHeaders = entries.GroupBy(e => e.Raw.Length > 0 ? e.Raw[0] : (byte)0)
                                       .ToDictionary(g => g.Key, g => g.Count());

        // Conta o que acontece se aplicarmos ENCRYPT em pacotes que ja comecam com C1/C2/C3/C4
        var encryptedC1C2 = entries.Where(e => e.Raw.Length > 0 && (e.Raw[0] == 0xC1 || e.Raw[0] == 0xC2 || e.Raw[0] == 0xC3 || e.Raw[0] == 0xC4))
                                   .Select(e => GameServerCrypto.Encrypt(e.Raw))
                                   .ToList();
        var encryptedHeaders = encryptedC1C2.GroupBy(e => e[0])
                                              .ToDictionary(g => g.Key, g => g.Count());

        var sb = new StringBuilder();
        sb.AppendLine($"Total de pacotes: {entries.Count}");
        sb.AppendLine();
        sb.AppendLine("Headers no log original (provavelmente misto de decifrado+cifrado):");
        foreach (var h in originalHeaders.Keys.Order())
        {
            var expectedPlain = HeaderToPlain(h);
            sb.AppendLine($"  0x{h:X2}: {originalHeaders[h]} pacotes -> se decifrar vira {expectedPlain}");
        }

        sb.AppendLine();
        sb.AppendLine("Se pacotes C1/C2/C3/C4 no log fossem cifrados com 0x07/0x36, seus headers na wire seriam:");
        foreach (var h in encryptedHeaders.Keys.Order())
            sb.AppendLine($"  0x{h:X2}: {encryptedHeaders[h]} ocorrencias");

        _output.WriteLine(sb.ToString());

        // Salva relatorio
        var reportPath = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "header_pattern_report.txt");
        File.WriteAllText(reportPath, sb.ToString());
        _output.WriteLine($"Relatorio salvo em: {reportPath}");
    }

    private static string HeaderToPlain(byte b)
    {
        var dec = GameServerCrypto.Decrypt(new[] { b });
        var enc = GameServerCrypto.Encrypt(new[] { b });
        return $"dec=0x{dec[0]:X2} enc=0x{enc[0]:X2}";
    }
}
