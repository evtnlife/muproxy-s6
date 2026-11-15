using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Text.RegularExpressions;
using WinFormsMitm.Events;

namespace WinFormsMitm
{
    /// <summary>
    /// Proxy TCP loopback puro (pass-through).
    /// NÃO faz criptografia — o byp.dll cuida de toda a cifra/injeção.
    /// </summary>
    public class TcpLoopbackProxy : IDisposable
    {
        private readonly List<Listener> _listeners = new();
        private readonly object _listenersLock = new();
        private CancellationTokenSource _cts = new();
        private bool _started = false;

        public bool IsRunning => _listeners.Any(l => l.IsRunning);

        public event Action<string>? OnLog;
        public event Action<string, byte[]>? OnClientToServer;
        public event Action<string, byte[]>? OnServerToClient;
        // Focado quando o pacote C116 (ConnectToServer) revela a porta real do GS.
        public event Action<int>? OnGsPortDetected;

        public TcpLoopbackProxy() { }

        private void Log(string msg) => OnLog?.Invoke(msg);

        public bool HasListener(int listenPort)
        {
            lock (_listenersLock) return _listeners.Any(l => l._listenPort == listenPort);
        }

        public void AddListener(int listenPort, string realHost, int realPort, bool rewriteServerList = false)
        {
            Listener l;
            bool startNow;
            CancellationToken ct;
            lock (_listenersLock)
            {
                if (_listeners.Any(x => x._listenPort == listenPort)) return; // já existe
                l = new Listener(listenPort, realHost, realPort, rewriteServerList, Log, OnClientToServer, OnServerToClient,
                    port => OnGsPortDetected?.Invoke(port));
                _listeners.Add(l);
                startNow = _started;
                ct = _cts.Token;
            }
            // Se o proxy JÁ FOI STARTADO, inicia o listener imediatamente
            // (necessário para a porta do GS detectada via C116, que chega
            // depois do Start()). Antes do Start(), o listener é iniciado
            // pelo próprio Start() — iniciar aqui causaria double-start.
            if (startNow && !ct.IsCancellationRequested)
            {
                try { l.Start(ct); }
                catch (Exception ex) { Log($"[PROXY] Falha ao iniciar listener {listenPort}: {ex.Message}"); }
            }
        }

        public void Start()
        {
            Stop();
            _cts = new CancellationTokenSource();
            List<Listener> snapshot;
            lock (_listenersLock) { snapshot = new List<Listener>(_listeners); _started = true; }
            foreach (var l in snapshot) l.Start(_cts.Token);
        }

        public void Stop()
        {
            _cts.Cancel();
            List<Listener> snapshot;
            lock (_listenersLock) { snapshot = new List<Listener>(_listeners); _started = false; }
            foreach (var l in snapshot) l.Stop();
        }

        public void Dispose()
        {
            Stop();
            _cts.Dispose();
        }

        private class Listener
        {
            private TcpListener? _listener;
            private CancellationTokenSource? _localCts;
            private Task? _listenTask;
            public readonly int _listenPort;
            private readonly string _realHost;
            private readonly int _realPort;
            private readonly bool _rewrite;
            private readonly Action<string> _log;
            private readonly Action<string, byte[]>? _onC2S;
            private readonly Action<string, byte[]>? _onS2C;
            private readonly Action<int>? _onGsPortDetected;
            private NetworkStream? _clientStream;
            private NetworkStream? _serverStream;
            private readonly object _lock = new();
            private int _detectedGsPort = 0;
            // Cauda dos ultimos 21 bytes da leitura S->C anterior — cobre o caso
            // do C116 (22 bytes) ser dividido entre dois reads TCP.
            private byte[] _c116Tail = Array.Empty<byte>();

            public bool IsRunning => _listener != null;

            public Listener(int listenPort, string realHost, int realPort, bool rewrite,
                Action<string> log, Action<string, byte[]>? onC2S, Action<string, byte[]>? onS2C,
                Action<int>? onGsPortDetected)
            {
                _listenPort = listenPort;
                _realHost = realHost;
                _realPort = realPort;
                _rewrite = rewrite;
                _log = log;
                _onC2S = onC2S;
                _onS2C = onS2C;
                _onGsPortDetected = onGsPortDetected;
            }

            // ====================================================================
            // Detecção da porta do GS via C116 (ConnectToServer).
            // O login server NAO cifra o trafego (em claro). Formato real (22B):
            //   C1 16 [charID 2B LE] [IP 16B null-padded] [porta 2B LE]
            // Exemplo: C1 16 F4 03 "145.239.149.84\0.." 4D B3 -> porta 45901.
            // Escaneamos o buffer pela assinatura em claro C1 16, validamos que
            // o campo IP comeca com digito e lemos a porta no offset 20 (LE).
            // Retorna a porta detectada ou 0 se não encontrada.
            // ====================================================================
            private static int TryDetectGsPortFromC116(byte[] data, int offset, int len)
            {
                const int C116_LEN = 22;
                if (len < C116_LEN) return 0;
                int end = offset + len;
                for (int i = offset; i + C116_LEN <= end; ++i)
                {
                    if (data[i] != 0xC1 || data[i + 1] != 0x16) continue;

                    // Campo IP (offset 4) deve comecar com digito (primeiro char de um IPv4)
                    if (data[i + 4] < '0' || data[i + 4] > '9') continue;

                    // Porta no offset 20 (2B header + 2B charID + 16B IP), little-endian
                    int port = (data[i + 21] << 8) | data[i + 20];
                    if (port < 1024 || port > 65535) continue;
                    return port;
                }
                return 0;
            }

            public void Start(CancellationToken parentCt)
            {
                Stop();
                _localCts = CancellationTokenSource.CreateLinkedTokenSource(parentCt);
                _listener = new TcpListener(IPAddress.Loopback, _listenPort);
                _listener.Start();
                _listenTask = Task.Run(ListenLoop, _localCts.Token);
                _log($"[PROXY] Ouvindo 127.0.0.1:{_listenPort} -> {_realHost}:{_realPort}" + (_rewrite ? " (rewrite server list)" : ""));
            }

            public void Stop()
            {
                _localCts?.Cancel();
                try { _listener?.Stop(); } catch { }
                try { _listener?.Server.Dispose(); } catch { }
                _listener = null;
                try { _listenTask?.Wait(500); } catch { }
                _listenTask = null;
                _clientStream = null;
                _serverStream = null;
            }

            private async Task ListenLoop()
            {
                while (_localCts != null && !_localCts.IsCancellationRequested)
                {
                    TcpClient? client = null;
                    try
                    {
                        client = await _listener!.AcceptTcpClientAsync().WaitAsync(_localCts.Token);
                        _ = HandleClient(client);
                    }
                    catch (OperationCanceledException) { break; }
                    catch (Exception ex)
                    {
                        _log($"[PROXY] Accept error on {_listenPort}: {ex.Message}");
                    }
                }
            }

            private async Task HandleClient(TcpClient client)
            {
                using (client)
                {
                    TcpClient? server = null;
                    // CTS por conexao: quando uma direcao do relay termina (EOF),
                    // cancelamos para a outra direcao receber OperationCanceledException
                    // em vez de IOException "aborted" (teardown limpo, sem erro no log).
                    using var connCts = CancellationTokenSource.CreateLinkedTokenSource(_localCts!.Token);
                    try
                    {
                        server = new TcpClient();
                        await server.ConnectAsync(_realHost, _realPort).WaitAsync(connCts.Token);

                        _log($"[PROXY] Conectado ao servidor real {_realHost}:{_realPort} para cliente 127.0.0.1:{_listenPort}");

                        _clientStream = client.GetStream();
                        _serverStream = server.GetStream();

                        var t1 = Relay(_clientStream, _serverStream, "C->S", false, connCts.Token, _listenPort.ToString());
                        var t2 = Relay(_serverStream, _clientStream, "S->C", _rewrite, connCts.Token, _listenPort.ToString());

                        await Task.WhenAny(t1, t2);
                        connCts.Cancel(); // encerra a outra direcao de forma limpa
                        // Aguarda as duas direcoes terminarem ANTES de dispor os sockets
                        // (o finally/using abaixo). Se dispuser antes, o ReadAsync pendente
                        // falha com SocketException "aborted" em vez de OperationCanceled.
                        try { await Task.WhenAll(t1, t2); } catch { }
                    }
                    catch (Exception ex)
                    {
                        _log($"[PROXY] Client handler {_listenPort}: {ex.Message}");
                    }
                    finally
                    {
                        try { server?.Close(); } catch { }
                        try { server?.Dispose(); } catch { }
                    }
                }
            }

            private async Task Relay(NetworkStream from, NetworkStream to, string dir, bool rewrite, CancellationToken ct, string tag)
            {
                byte[] buf = new byte[8192];
                long totalBytes = 0;
                try
                {
                    while (!ct.IsCancellationRequested)
                    {
                        int read = await from.ReadAsync(buf, 0, buf.Length, ct);
                        if (read <= 0)
                        {
                            // Log de EOF so quando a direcao realmente transportou dados.
                            // Conexoes keepalive/probe (sem trafego) fecham em silencio.
                            if (totalBytes > 0)
                                _log($"[PROXY] Relay {tag}:{dir}: EOF apos {totalBytes} bytes");
                            break;
                        }
                        totalBytes += read;
                        var copy = new byte[read];
                        Buffer.BlockCopy(buf, 0, copy, 0, read);

                        // Detecta a porta do GS via C116 no trafego S->C da conexao de login.
                        // Precisa acontecer ANTES de forwardar para o cliente, assim o
                        // listener do GS ja existe quando o cliente decidir conectar.
                        // Usa a cauda da leitura anterior para cobrir C116 dividido
                        // entre dois reads TCP.
                        if (dir == "S->C" && rewrite && _detectedGsPort == 0)
                        {
                            byte[] scan;
                            if (_c116Tail.Length > 0)
                            {
                                scan = new byte[_c116Tail.Length + read];
                                Buffer.BlockCopy(_c116Tail, 0, scan, 0, _c116Tail.Length);
                                Buffer.BlockCopy(copy, 0, scan, _c116Tail.Length, read);
                            }
                            else
                            {
                                scan = copy;
                            }
                            int gsPort = TryDetectGsPortFromC116(scan, 0, scan.Length);
                            if (gsPort > 0)
                            {
                                _detectedGsPort = gsPort;
                                _c116Tail = Array.Empty<byte>();
                                _log($"[C116] Porta do GS detectada = {gsPort}");
                                _onGsPortDetected?.Invoke(gsPort);
                            }
                            else
                            {
                                // Mantem os ultimos 21 bytes para a proxima leitura
                                int keep = Math.Min(21, read);
                                if (keep > 0)
                                {
                                    _c116Tail = new byte[keep];
                                    Buffer.BlockCopy(copy, read - keep, _c116Tail, 0, keep);
                                }
                            }
                        }

                        // Log do trafego do game server para debug
                        if (tag == "55902")
                        {
                            _log($"[RELAY] {tag}:{dir} len={read} hex={BitConverter.ToString(copy).Replace("-", " ")}");
                        }

                        if (dir == "C->S")
                            _onC2S?.Invoke($"{tag}:{dir}", copy);
                        else
                            _onS2C?.Invoke($"{tag}:{dir}", copy);

                        if (rewrite)
                        {
                            var original = (byte[])copy.Clone();
                            RewriteServerListPayload(copy, _realHost);
                            if (!copy.SequenceEqual(original))
                            {
                                _log($"[REWRITE] Server list payload de {original.Length} bytes alterado");
                                _log($"[REWRITE] ORIGINAL hex: {BitConverter.ToString(original).Replace("-", " ")}");
                                _log($"[REWRITE] REWRITE hex: {BitConverter.ToString(copy).Replace("-", " ")}");
                            }
                        }

                        await to.WriteAsync(copy, 0, read, ct);
                    }
                }
                catch (OperationCanceledException) { }
                catch (IOException) { /* conexao encerrada normalmente (teardown) */ }
                catch (Exception ex)
                {
                    _log($"[PROXY] Relay {tag}:{dir}: {ex.Message}");
                }
            }

            private static void RewriteServerListPayload(byte[] data, string realHost)
            {
                var enc = Encoding.GetEncoding("iso-8859-1");
                string text = enc.GetString(data);
                // Apenas substitui IPs de servidor (nao DNS, nao mascara, etc.)
                // O game server real e 54.36.205.234; substituimos exatamente esse IP por 127.0.0.1
                var specificIp = realHost.Trim();
                var srcBytes = enc.GetBytes(specificIp);
                var dstBytes = enc.GetBytes("127.0.0.1");
                if (srcBytes.Length == dstBytes.Length)
                {
                    for (int i = 0; i <= data.Length - srcBytes.Length; i++)
                    {
                        bool ok = true;
                        for (int j = 0; j < srcBytes.Length; j++)
                            if (data[i + j] != srcBytes[j]) { ok = false; break; }
                        if (ok)
                        {
                            for (int j = 0; j < dstBytes.Length; j++) data[i + j] = dstBytes[j];
                        }
                    }
                }
            }
        }
    }
}
