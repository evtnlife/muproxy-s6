using System.Diagnostics;
using System.Runtime.InteropServices;

namespace WinFormsMitm.Services
{
    /// <summary>
    /// Pasta de sessão por instância: cada instância do MuProxy2 tem o seu
    /// diretório de trabalho (CWD) próprio dentro da pasta do jogo.
    ///
    /// POR QUE
    /// -------
    /// O cliente BlackrockMU lê quase tudo **relativo ao CWD**:
    ///
    ///   .\Settings.ini              (conta/senha do auto-login!)
    ///   Data\...                    (dados do jogo: map, som, scripts)
    ///   Custom\..., Interface\...   (interface)
    ///   .\ScriptSystem.premium
    ///
    /// Como as duas instâncias apontavam para o mesmo CWD, elas dividiam o
    /// mesmo Settings.ini — logo, o **mesmo usuário/senha** — e o servidor
    /// derrubava a segunda conta ("tomo dc"). Com uma pasta por instância:
    ///
    ///   - cada uma tem o seu Settings.ini (conta própria);
    ///   - as pastas de dados do jogo são JUNÇÕES para as originais, então não
    ///     duplicamos hundreds of MB nem perdemos as atualizações do jogo;
    ///   - o log do byp.dll também cai na pasta da sessão (MUPROXY_SESSION).
    ///
    /// Observação: o main.exe continua sendo executado a partir da pasta do
    /// jogo (o caminho do executável não muda), só o CWD é o da sessão.
    /// </summary>
    public static class SessionFolder
    {
        /// <summary>
        /// Cria (se preciso) a pasta de sessão da instância e devolve o caminho.
        /// </summary>
        public static string Prepare(string gameDir, int instanceBase)
        {
            string root = Path.Combine(gameDir, "sessions");
            string dir = Path.Combine(root, "base" + instanceBase);
            Directory.CreateDirectory(dir);

            // Pastas do jogo -> junção (o conteúdo continua o do jogo).
            foreach (string sub in Directory.GetDirectories(gameDir))
            {
                string name = Path.GetFileName(sub);
                if (!ShouldLink(name)) continue;

                string link = Path.Combine(dir, name);
                if (Directory.Exists(link) || File.Exists(link)) continue;
                CreateJunction(link, sub);
            }

            // Arquivos de configuração do cliente -> cópia (o Settings.ini é o
            // que guarda a conta do auto-login: cada instância precisa do seu).
            foreach (string name in new[] { "Settings.ini", "ScriptSystem.premium", "main.premium" })
            {
                string src = Path.Combine(gameDir, name);
                string dst = Path.Combine(dir, name);
                if (!File.Exists(src)) continue;
                try
                {
                    // Só copia se ainda não existir: o cliente pode ter
                    // reescrito o arquivo (SaveData) com a conta da sessão.
                    if (!File.Exists(dst)) File.Copy(src, dst, true);
                }
                catch { }
            }

            // O cliente valida o CRC do main.exe (e de outros arquivos) ABRINDO
            // o ARQUIVO pelo NOME, ou seja, RELATIVO AO CWD — se o main.exe não
            // estiver na pasta de sessão, o CRC falha e o cliente abre um
            // MessageBox "Main.exe CRC error!" e se mata.
            // Solução: hardlink dos arquivos do jogo na pasta da sessão
            // (mesmo conteúdo, custo zero de espaço) — refeito a cada launch
            // para acompanhar qualquer atualização do launcher.
            foreach (string file in Directory.GetFiles(gameDir))
            {
                string name = Path.GetFileName(file);
                if (!ShouldLinkFile(name)) continue;

                string dst = Path.Combine(dir, name);
                try
                {
                    if (File.Exists(dst)) File.Delete(dst);
                    CreateHardLink(dst, file);
                }
                catch { }
            }

            return dir;
        }

        /// <summary>
        /// Arquivos do jogo que a sessão deve expor. Os logs ficam de fora:
        /// o byp.dll escreve o log dele na pasta da sessão e não pode acabar
        /// no mesmo arquivo da outra instância.
        /// </summary>
        private static bool ShouldLinkFile(string name)
        {
            string lower = name.ToLowerInvariant();
            if (lower.EndsWith(".log")) return false;
            if (lower == "settings.ini") return false;   // cópia por instância
            if (lower == "scriptsystem.premium" || lower == "main.premium") return false;
            return true;
        }

        /// <summary>
        /// Cria um hardlink (mesmo arquivo, sem copiar os dados). Se falhar,
        /// copia o arquivo.
        /// </summary>
        private static void CreateHardLink(string link, string target)
        {
            if (CreateHardLinkNative(link, target)) return;
            try { File.Copy(target, link, true); } catch { }
        }

        [DllImport("kernel32.dll", EntryPoint = "CreateHardLinkW",
                   CharSet = CharSet.Unicode, SetLastError = true)]
        private static extern bool CreateHardLinkNative(string link, string target);

        /// <summary>
        /// Grava a conta/senha do auto-login no Settings.ini da sessão.
        ///
        /// O cliente lê o Settings.ini do CWD (.\Settings.ini) e usa a seção
        /// [AutoLogin] para logar sozinho. Como cada instância tem a sua
        /// pasta, cada uma pode ter a SUA conta — assim as duas instâncias
        /// não disputam a mesma sessão do servidor.
        ///
        /// O resto do arquivo é preservado (o resto das seções do cliente).
        /// </summary>
        public static bool WriteAutoLogin(string sessionDir, string user, string password)
        {
            if (string.IsNullOrWhiteSpace(sessionDir)) return false;
            string path = Path.Combine(sessionDir, "Settings.ini");
            try
            {
                var lines = File.Exists(path)
                    ? new List<string>(File.ReadAllLines(path))
                    : new List<string>();

                int section = lines.FindIndex(l => l.Trim().Equals("[AutoLogin]", StringComparison.OrdinalIgnoreCase));
                if (section < 0)
                {
                    lines.Add("[AutoLogin]");
                    lines.Add($"User= \"{user}\"");
                    lines.Add($"Password= \"{password}\"");
                    lines.Add("SaveData= 1");
                }
                else
                {
                    // Reescreve User/Password dentro da seção; se não existirem,
                    // insere no começo da seção.
                    int userAt = -1, passAt = -1, insertAt = section + 1;
                    for (int i = section + 1; i < lines.Count; i++)
                    {
                        string t = lines[i].Trim();
                        if (t.StartsWith("[")) break;
                        if (t.StartsWith("User", StringComparison.OrdinalIgnoreCase)) userAt = i;
                        else if (t.StartsWith("Password", StringComparison.OrdinalIgnoreCase)) passAt = i;
                    }
                    if (userAt >= 0) lines[userAt] = $"User= \"{user}\"";
                    else lines.Insert(insertAt++, $"User= \"{user}\"");

                    if (passAt >= 0) lines[passAt] = $"Password= \"{password}\"";
                    else lines.Insert(insertAt++, $"Password= \"{password}\"");
                }

                File.WriteAllLines(path, lines);
                return true;
            }
            catch
            {
                return false;
            }
        }

        /// <summary>
        /// Pastas do jogo que valem a pena expor na sessão. As pastas de
        /// trabalho/análise do projeto não são recursos do cliente.
        /// </summary>
        private static bool ShouldLink(string name)
        {
            switch (name.ToLowerInvariant())
            {
                case "sessions":
                case "byp":
                case ".git":
                case ".agents":
                case ".novamind":
                case "node_modules":
                case "muemu-master":
                case "openmu-master":
                case "openspec":
                case "blackrockmu.exe.webview2":
                    return false;
                default:
                    return true;
            }
        }

        /// <summary>
        /// Cria uma junção de diretório (mklink /J). Não precisa de admin e
        /// não duplica os dados do jogo.
        /// </summary>
        private static bool CreateJunction(string link, string target)
        {
            try
            {
                var psi = new ProcessStartInfo("cmd.exe")
                {
                    UseShellExecute = false,
                    CreateNoWindow = true,
                    RedirectStandardOutput = true,
                    RedirectStandardError = true,
                };
                psi.ArgumentList.Add("/c");
                psi.ArgumentList.Add("mklink");
                psi.ArgumentList.Add("/J");
                psi.ArgumentList.Add(link);
                psi.ArgumentList.Add(target);

                using var p = Process.Start(psi);
                if (p == null) return false;
                p.WaitForExit(5000);
                return p.ExitCode == 0;
            }
            catch
            {
                return false;
            }
        }
    }
}
