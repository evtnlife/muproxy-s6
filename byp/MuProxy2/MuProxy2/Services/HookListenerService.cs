using System.Net;
using System.Net.Sockets;
using System.Text;

namespace WinFormsMitm.Services
{
    /// <summary>
    /// Gerencia os TCP listeners que recebem conexões do byp.dll.
    /// Isolado: não sabe nada de UI, Monster, Attack — só encaminha
    /// linhas cruas para o DllCommandService.
    /// </summary>
    public class HookListenerService : IDisposable
    {
        private readonly DllCommandService _dll;
        private readonly Action<string>? _log;

        private CancellationTokenSource? _cts;
        private TcpListener? _listenerCmd;   // 44412 + base
        private TcpListener? _listenerPkt;   // 44413 + base
        private int _pktPort = 44413;
        private int _instanceBase;           // base desta instância (0, 100, ...)
        private Task? _taskCmd;
        private Task? _taskPkt;
        private readonly List<TcpClient> _clientsCmd = new();
        private readonly List<TcpClient> _clientsPkt = new();
        private readonly Dictionary<int, DateTime> _refusalLog = new();

        public bool IsRunning { get; private set; }

        /// <summary>
        /// PID do main.exe que ESTA instância lançou (0 = nenhum).
        /// No handshake HELLO o byp se identifica com esse PID: um byp com
        /// PID diferente NÃO é desta instância e é recusado.
        /// </summary>
        public int ExpectedGamePid { get; set; }

        public HookListenerService(DllCommandService dll, Action<string>? log = null)
        {
            _dll = dll;
            _log = log;
        }

        public void Start(int cmdPort = 44412, int pktPort = 44413, int instanceBase = 0)
        {
            Stop();
            _cts = new CancellationTokenSource();
            _instanceBase = instanceBase;

            _listenerCmd = new TcpListener(IPAddress.Loopback, cmdPort);
            _listenerCmd.Start();
            _taskCmd = Task.Run(() => AcceptLoop(_listenerCmd, _clientsCmd, HandleCmdClient, _cts.Token));

            _listenerPkt = new TcpListener(IPAddress.Loopback, pktPort);
            _listenerPkt.Start();
            _taskPkt = Task.Run(() => AcceptLoop(_listenerPkt, _clientsPkt, HandlePktClient, _cts.Token));
            _pktPort = pktPort;

            IsRunning = true;
            _log?.Invoke($"[HOOK] Listeners iniciados ({cmdPort} CMD, {pktPort} PKT, base {instanceBase})");
        }

        public void Stop()
        {
            IsRunning = false;
            _cts?.Cancel();
            try { _listenerCmd?.Stop(); } catch { }
            try { _listenerPkt?.Stop(); } catch { }
            _listenerCmd = null;
            _listenerPkt = null;

            lock (_clientsCmd) { foreach (var c in _clientsCmd) try { c.Close(); } catch { } _clientsCmd.Clear(); }
            lock (_clientsPkt) { foreach (var c in _clientsPkt) try { c.Close(); } catch { } _clientsPkt.Clear(); }

            _log?.Invoke("[HOOK] Listeners parados");
        }

        private async Task AcceptLoop(TcpListener listener, List<TcpClient> clients,
            Func<TcpClient, CancellationToken, Task> handler, CancellationToken ct)
        {
            try
            {
                while (!ct.IsCancellationRequested)
                {
                    var client = await listener.AcceptTcpClientAsync().WaitAsync(ct);
                    ConfigureKeepAlive(client);
                    lock (clients) { clients.Add(client); }
                    _ = handler(client, ct);
                }
            }
            catch (OperationCanceledException) { }
            catch (Exception ex)
            {
                if (!ct.IsCancellationRequested)
                    _log?.Invoke($"[HOOK] Accept error: {ex.Message}");
            }
        }

        private static void ConfigureKeepAlive(TcpClient client)
        {
            try
            {
                client.Client.SetSocketOption(SocketOptionLevel.Socket, SocketOptionName.KeepAlive, true);
                byte[] ka = new byte[12];
                BitConverter.GetBytes(1).CopyTo(ka, 0);
                BitConverter.GetBytes(5000).CopyTo(ka, 4);
                BitConverter.GetBytes(1000).CopyTo(ka, 8);
                client.Client.IOControl(IOControlCode.KeepAliveValues, ka, null);
            }
            catch { }
        }

        private async Task HandleCmdClient(TcpClient client, CancellationToken ct)
        {
            StreamWriter? writer = null;
            try
            {
                var stream = client.GetStream();
                using var reader = new StreamReader(stream, Encoding.UTF8);
                // Sem BOM: o byp faz o strip do BOM só na primeira conexão
                // (flag de estado), então reconectar com BOM quebraria o
                // protocolo (a tag da 1a linha nao casaria).
                writer = new StreamWriter(stream, new UTF8Encoding(false)) { AutoFlush = true };

                // ---- Handshake de identidade -------------------------------
                // O byp se apresenta logo ao conectar: "HELLO|pid=..|base=..".
                // Só aceitamos o byp que pertence a ESTA instância: base igual
                // e (se esta instância lançou um jogo) PID igual. Sem isso um
                // main.exe que caiu na porta errada seria controlado pela
                // janela errada — o "MuProxy1 = Main.exe 2".
                string? first = await reader.ReadLineAsync().WaitAsync(ct);
                if (first == null) return;

                if (!first.StartsWith("HELLO|", StringComparison.Ordinal))
                {
                    // byp antigo (sem HELLO): aceita, mas avisa no log.
                    _log?.Invoke("[HOOK] byp sem HELLO (byp.dll antigo) — aceitando assim mesmo");
                    _dll.ProcessLine(first);
                }
                else
                {
                    int pid = ParseField(first, "pid");
                    int bypBase = ParseField(first, "base");
                    bool ok = bypBase == _instanceBase;
                    if (ok && ExpectedGamePid > 0 && pid != ExpectedGamePid) ok = false;

                    if (ok)
                    {
                        _log?.Invoke($"[HOOK] byp aceito (pid={pid}, base={bypBase})");
                    }
                    else
                    {
                        if (ShouldLogRefusal(pid))
                            _log?.Invoke($"[HOOK] byp RECUSADO (pid={pid}, base={bypBase}) — esta instância é a base " +
                                         $"{_instanceBase}" +
                                         (ExpectedGamePid > 0 ? $" e esperava o pid {ExpectedGamePid}" : "") +
                                         ". Esse main.exe pertence a outra janela do MuProxy2.");
                    }

                    try { await writer.WriteLineAsync($"HELLO_ACK|ok={(ok ? 1 : 0)}|base={_instanceBase}"); }
                    catch { }
                    if (!ok) return;   // fecha a conexão: não é nosso byp
                }

                _dll.RegisterWriter(writer);

                while (!ct.IsCancellationRequested)
                {
                    string? line = null;
                    try { line = await reader.ReadLineAsync().WaitAsync(ct); }
                    catch (OperationCanceledException) { break; }
                    catch (IOException) { break; }
                    catch (Exception ex)
                    {
                        _log?.Invoke($"[HOOK-CMD] Read error: {ex.Message}");
                        break;
                    }
                    if (line == null) break;
                    _dll.ProcessLine(line);
                }
            }
            catch (OperationCanceledException) { }
            finally
            {
                if (writer != null) _dll.UnregisterWriter(writer);
                try { client.Close(); } catch { }
                lock (_clientsCmd) { _clientsCmd.Remove(client); }
            }
        }

        /// <summary>Lê um campo "chave=valor" de uma linha do protocolo.</summary>
        private static int ParseField(string line, string key)
        {
            foreach (var part in line.Split('|'))
            {
                int eq = part.IndexOf('=');
                if (eq <= 0) continue;
                if (!part.Substring(0, eq).Equals(key, StringComparison.Ordinal)) continue;
                if (int.TryParse(part.Substring(eq + 1), out int v)) return v;
            }
            return -1;
        }

        /// <summary>
        /// Log de recusa com throttle: o byp reconecta no canal de comandos a
        /// cada segundo e seria recusado toda vez.
        /// </summary>
        private bool ShouldLogRefusal(int pid)
        {
            lock (_refusalLog)
            {
                var now = DateTime.UtcNow;
                if (_refusalLog.TryGetValue(pid, out var last) && now - last < TimeSpan.FromSeconds(30))
                    return false;
                _refusalLog[pid] = now;
                return true;
            }
        }

        private async Task HandlePktClient(TcpClient client, CancellationToken ct)
        {
            try
            {
                _log?.Invoke($"[HOOK-PKT] byp.dll PKT conectado ({_pktPort})");
                var stream = client.GetStream();
                using var reader = new StreamReader(stream, Encoding.UTF8);

                while (!ct.IsCancellationRequested)
                {
                    string? line = null;
                    try { line = await reader.ReadLineAsync().WaitAsync(ct); }
                    catch (OperationCanceledException) { break; }
                    catch (IOException) { break; }
                    catch (Exception ex)
                    {
                        _log?.Invoke($"[HOOK-PKT] Read error: {ex.Message}");
                        break;
                    }
                    if (line == null) break;
                    _dll.ProcessLine(line);
                }
            }
            catch (OperationCanceledException) { }
            finally
            {
                _log?.Invoke($"[HOOK-PKT] byp.dll PKT desconectado ({_pktPort})");
                try { client.Close(); } catch { }
                lock (_clientsPkt) { _clientsPkt.Remove(client); }
            }
        }

        public void Dispose()
        {
            Stop();
            _cts?.Dispose();
        }
    }
}
