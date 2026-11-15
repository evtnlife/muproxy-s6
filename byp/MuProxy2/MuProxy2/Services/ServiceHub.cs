using System.Net.Sockets;
using System.Text;

namespace WinFormsMitm.Services
{
    /// <summary>
    /// Centraliza o envio de comandos para o byp.dll e gerencia o ciclo de vida
    /// de todos os serviços do jogo (AttackService, MonsterService, etc).
    /// </summary>
    public class ServiceHub : IDisposable
    {
        private readonly List<IGameService> _services = new();
        private readonly List<StreamWriter> _cmdWriters = new();
        private readonly object _writerLock = new();

        public event Action<string>? OnLog;

        // ---- Registro de serviços ----

        public void Register(IGameService service)
        {
            _services.Add(service);
            OnLog?.Invoke($"[SVC] Serviço registrado: {service.Name}");
        }

        public T? Get<T>() where T : class, IGameService
        {
            return _services.OfType<T>().FirstOrDefault();
        }

        // ---- Gerenciamento de writers TCP (porta 44412) ----

        public void AddCmdWriter(StreamWriter writer)
        {
            lock (_writerLock) { _cmdWriters.Add(writer); }
        }

        public void RemoveCmdWriter(StreamWriter writer)
        {
            lock (_writerLock) { _cmdWriters.RemoveAll(w => ReferenceEquals(w, writer)); }
        }

        /// <summary>Envia uma linha de comando para TODOS os writers CMD ativos.</summary>
        public int BroadcastCommand(string line)
        {
            List<StreamWriter> snapshot;
            lock (_writerLock) { snapshot = new List<StreamWriter>(_cmdWriters); }
            int sent = 0;
            foreach (var w in snapshot)
            {
                try { w.WriteLine(line); sent++; }
                catch { /* conexão morreu */ }
            }
            return sent;
        }

        /// <summary>Envia CMD para o byp.dll. Formato: "CMD &lt;OPCODE&gt; &lt;SUBCMD&gt; &lt;params...&gt;"</summary>
        public void SendCommand(string command)
        {
            int sent = BroadcastCommand(command);
            if (sent == 0)
                OnLog?.Invoke($"[CMD] ERRO: byp.dll nao conectada via TCP 44412");
            else
                OnLog?.Invoke($"[CMD] →byp.dll {command} ({sent} conexao(oes))");
        }

        /// <summary>Envia pacote hex para injeção no game server.</summary>
        public void SendInject(string hex)
        {
            int sent = BroadcastCommand($"SEND Client {hex}");
            if (sent == 0)
                OnLog?.Invoke("[SEND] ERRO: byp.dll nao conectada");
            else
                OnLog?.Invoke($"[SEND] →byp.dll {hex}");
        }

        // ---- Ciclo de vida ----

        public void NotifyDllConnected()
        {
            OnLog?.Invoke("[SVC] DLL conectada, notificando serviços...");
            foreach (var svc in _services)
            {
                try { svc.OnDllConnected(); }
                catch (Exception ex) { OnLog?.Invoke($"[SVC] Erro em {svc.Name}.OnDllConnected: {ex.Message}"); }
            }
        }

        public void NotifyDllDisconnected()
        {
            OnLog?.Invoke("[SVC] DLL desconectada, notificando serviços...");
            foreach (var svc in _services)
            {
                try { svc.OnDllDisconnected(); }
                catch (Exception ex) { OnLog?.Invoke($"[SVC] Erro em {svc.Name}.OnDllDisconnected: {ex.Message}"); }
            }
        }

        public void Dispose()
        {
            foreach (var svc in _services)
            {
                try { svc.Shutdown(); }
                catch (Exception ex) { OnLog?.Invoke($"[SVC] Erro em {svc.Name}.Shutdown: {ex.Message}"); }
            }
            _services.Clear();
            lock (_writerLock) { _cmdWriters.Clear(); }
        }
    }
}
