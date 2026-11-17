using System.IO;

namespace WinFormsMitm.Tests
{
    /// <summary>
    /// Localiza o log de captura (byp_packets.log) usado pelos testes de
    /// analise de pacote. O arquivo e um ARTEFATO: pode nao existir (foi
    /// movido/limpo). Nesse caso Find() devolve null e o teste sai sem
    /// falhar — nao ha o que analisar.
    /// </summary>
    internal static class TestLogLocator
    {
        private static readonly string[] Candidates =
        {
            Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "byp_packets.log"),
            @"F:\blackrockmu\logs_backup_2026-08-05\byp_packets.log",
            @"F:\blackrockmu\byp_packets.log",
        };

        /// <summary>Caminho do log de captura, ou null se nao houver.</summary>
        public static string? Find()
        {
            foreach (var path in Candidates)
            {
                if (!string.IsNullOrEmpty(path) && File.Exists(path))
                    return path;
            }
            return null;
        }
    }
}
