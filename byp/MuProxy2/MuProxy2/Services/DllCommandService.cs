using System.Net.Sockets;
using System.Text;
using WinFormsMitm.Protocol;

namespace WinFormsMitm.Services
{
    /// <summary>
    /// Camada central de comunicação com o byp.dll via TCP.
    /// ÚNICO ponto que envia/recebe comandos. Todos os serviços
    /// (Monster, Attack, etc) usam este serviço.
    /// </summary>
    public class DllCommandService : IDisposable
    {
        private readonly object _writerLock = new();
        private readonly List<StreamWriter> _writers = new();
        private bool _disposed;

        // ---- Eventos de recebimento ----
        public event Action? OnDllConnected;
        public event Action? OnDllDisconnected;
        public event Action<string>? OnLogLine;
        public event Action<List<EntityMessage>>? OnMonsterList;
        public event Action<string, byte[]>? OnPacketSniffed;  // PKT decriptado do byp.dll

        // ---- Evento de log (para UI) ----
        public event Action<string>? OnLog;

        public bool HasConnection
        {
            get { lock (_writerLock) return _writers.Count > 0; }
        }

        // ---- Envio de comandos ----

        /// <summary>Envia linha de comando para TODOS os writers ativos.</summary>
        public int SendCommand(string line)
        {
            List<StreamWriter> snapshot;
            lock (_writerLock) { snapshot = new List<StreamWriter>(_writers); }
            int sent = 0;
            foreach (var w in snapshot)
            {
                try { w.WriteLine(line); sent++; }
                catch { /* conexão morta, será removida no handler */ }
            }
            return sent;
        }

        // ---- Gerenciamento de writers (chamado pelo HookListenerService) ----

        public void RegisterWriter(StreamWriter writer)
        {
            lock (_writerLock) { _writers.Add(writer); }
            OnDllConnected?.Invoke();
            OnLog?.Invoke("[DLL] byp.dll conectada");
        }

        public void UnregisterWriter(StreamWriter writer)
        {
            lock (_writerLock) { _writers.RemoveAll(w => ReferenceEquals(w, writer)); }
            OnDllDisconnected?.Invoke();
            OnLog?.Invoke("[DLL] byp.dll desconectada");
        }

        // ---- Processamento de linhas recebidas (chamado pelo HookListenerService) ----

        private readonly List<EntityMessage> _entityBuffer = new();

        public void ProcessLine(string line)
        {
            var msg = MessageRegistry.Parse(line);
            switch (msg)
            {
                case EntityMessage entity:
                    _entityBuffer.Add(entity);
                    break;

                case EntityBatchEnd batchEnd:
                    if (_entityBuffer.Count > 0)
                    {
                        var snapshot = new List<EntityMessage>(_entityBuffer);
                        _entityBuffer.Clear();
                        OnMonsterList?.Invoke(snapshot);
                    }
                    break;

                case LogEntryMessage log:
                    OnLogLine?.Invoke(log.Text);
                    break;

                case PktMessage pkt:
                    if (pkt.Data.Length > 0)
                        OnPacketSniffed?.Invoke(pkt.Direction, pkt.Data);
                    break;

                case null:
                    break; // desconhecido, ignora
            }
        }

        public void Dispose()
        {
            if (_disposed) return;
            _disposed = true;
            lock (_writerLock)
            {
                foreach (var w in _writers)
                {
                    try { w.Dispose(); } catch { }
                }
                _writers.Clear();
            }
        }
    }
}
