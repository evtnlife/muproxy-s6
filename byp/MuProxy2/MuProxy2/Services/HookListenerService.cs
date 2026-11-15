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
        private TcpListener? _listenerCmd;   // 44412
        private TcpListener? _listenerPkt;   // 44413
        private Task? _taskCmd;
        private Task? _taskPkt;
        private readonly List<TcpClient> _clientsCmd = new();
        private readonly List<TcpClient> _clientsPkt = new();

        public bool IsRunning { get; private set; }

        public HookListenerService(DllCommandService dll, Action<string>? log = null)
        {
            _dll = dll;
            _log = log;
        }

        public void Start()
        {
            Stop();
            _cts = new CancellationTokenSource();

            _listenerCmd = new TcpListener(IPAddress.Loopback, 44412);
            _listenerCmd.Start();
            _taskCmd = Task.Run(() => AcceptLoop(_listenerCmd, _clientsCmd, HandleCmdClient, _cts.Token));

            _listenerPkt = new TcpListener(IPAddress.Loopback, 44413);
            _listenerPkt.Start();
            _taskPkt = Task.Run(() => AcceptLoop(_listenerPkt, _clientsPkt, HandlePktClient, _cts.Token));

            IsRunning = true;
            _log?.Invoke("[HOOK] Listeners iniciados (44412 CMD, 44413 PKT)");
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
                writer = new StreamWriter(stream, Encoding.UTF8) { AutoFlush = true };
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

        private async Task HandlePktClient(TcpClient client, CancellationToken ct)
        {
            try
            {
                _log?.Invoke("[HOOK-PKT] byp.dll PKT conectado (44413)");
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
                _log?.Invoke("[HOOK-PKT] byp.dll PKT desconectado (44413)");
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
