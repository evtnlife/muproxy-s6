using System.Text;
using Xunit;
using Xunit.Abstractions;

namespace WinFormsMitm.Tests;

public class BruteForceAlgorithmTests
{
    private readonly ITestOutputHelper _output;

    public BruteForceAlgorithmTests(ITestOutputHelper output)
    {
        _output = output;
    }

    [Fact]
    public void Find_Algorithm_For_All_Packets()
    {
        var logPath = @"F:\blackrockmu\logs_backup_2026-08-05\byp_packets.log";
        Assert.True(File.Exists(logPath));

        var entries = PacketLogParser.Parse(logPath);
        Assert.NotEmpty(entries);

        // Agrupa por header original no log
        var byOriginalHeader = entries.GroupBy(e => e.Raw.Length > 0 ? e.Raw[0] : (byte)0)
                                       .OrderBy(g => g.Key)
                                       .ToList();

        var sb = new StringBuilder();
        sb.AppendLine($"Total de pacotes: {entries.Count}");
        sb.AppendLine("Distribuicao por header original no log:");
        foreach (var g in byOriginalHeader)
        {
            var header = g.Key;
            var count = g.Count();
            var pct = 100.0 * count / entries.Count;
            var sample = g.First();
            sb.AppendLine($"  0x{header:X2}: {count} ({pct:F2}%) - ex len={sample.Raw.Length} hex={GameServerCrypto.ToHex(sample.Raw).Substring(0, Math.Min(40, sample.Raw.Length * 2))}");
        }
        _output.WriteLine(sb.ToString());

        // Para cada header original, tenta todas as combinacoes (xor,sub) ate achar uma que vira C1/C2/C3/C4
        var targetHeaders = new HashSet<byte> { 0xC1, 0xC2, 0xC3, 0xC4 };
        var foundMappings = new Dictionary<byte, (byte xorKey, byte subKey, bool addFirst, byte resultHeader)>();

        foreach (var g in byOriginalHeader.Where(x => !targetHeaders.Contains(x.Key)))
        {
            byte original = g.Key;
            var sample = g.First();

            bool found = false;
            for (int xor = 0; xor <= 0xFF && !found; xor++)
            {
                for (int sub = 0; sub <= 0xFF && !found; sub++)
                {
                    foreach (bool addFirst in new[] { false, true })
                    {
                        var dec = CryptoBrute.Transform(sample.Raw, (byte)xor, (byte)sub, addFirst);
                        if (targetHeaders.Contains(dec[0]))
                        {
                            // valida nos proximos 5 pacotes do mesmo header original
                            var nextSamples = g.Take(5).Select(e => CryptoBrute.Transform(e.Raw, (byte)xor, (byte)sub, addFirst));
                            if (nextSamples.All(d => targetHeaders.Contains(d[0])))
                            {
                                foundMappings[original] = ((byte)xor, (byte)sub, addFirst, dec[0]);
                                found = true;
                                break;
                            }
                        }
                    }
                }
            }
        }

        var sb2 = new StringBuilder();
        sb2.AppendLine("Mapeamentos encontrados (header original -> chaves -> header Mu):");
        foreach (var kv in foundMappings.OrderBy(x => x.Key))
        {
            var mode = kv.Value.addFirst ? "(c + sub) ^ xor" : "(c ^ xor) - sub";
            sb2.AppendLine($"  0x{kv.Key:X2} -> xor=0x{kv.Value.xorKey:X2} sub=0x{kv.Value.subKey:X2} mode={mode} -> 0x{kv.Value.resultHeader:X2}");
        }
        _output.WriteLine(sb2.ToString());

        // Verifica se todos os headers originais foram mapeados para C1-C4
        var unmapped = byOriginalHeader.Select(g => g.Key).Where(h => !targetHeaders.Contains(h) && !foundMappings.ContainsKey(h)).ToList();
        if (unmapped.Any())
        {
            _output.WriteLine($"Headers nao mapeados: {string.Join(", ", unmapped.Select(h => $"0x{h:X2}"))}");
        }

        // Assertiva: todos os headers do log devem ser C1-C4 OU ter mapeamento encontrado
        Assert.True(unmapped.Count == 0, $"Faltam mapear headers: {string.Join(",", unmapped.Select(h => $"0x{h:X2}"))}");
    }
}
