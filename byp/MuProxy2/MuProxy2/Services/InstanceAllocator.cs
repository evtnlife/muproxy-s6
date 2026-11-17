using System.Net;
using System.Net.Sockets;

namespace WinFormsMitm.Services
{
    /// <summary>
    /// Aloca a "base" de uma instância do MuProxy2.
    ///
    /// Cada instância do MuProxy2 cuida de UM main.exe, e o byp.dll (que vive
    /// dentro do main.exe) conversa com a UI por TCP. Duas instâncias na
    /// mesma máquina não podem usar o mesmo par (endereço, porta), então cada
    /// uma fica num espaço próprio:
    ///
    ///   proxy do CS / do GS : 127.0.0.X  : 44411 / &lt;porta do GS&gt;
    ///                          (as PORTAS do servidor nunca mudam — o que muda
    ///                           é o loopback: 127.0.0.1, .2, .3, ...)
    ///   canal de comandos    : 127.0.0.1 : 44412 + base
    ///   canal de pacotes     : 127.0.0.1 : 44413 + base
    ///
    /// A base é escolhida testando o bind REAL das portas (nada de chute):
    /// se alguma estiver ocupada, tenta a próxima. O main.exe lançado pela
    /// instância herda a base pela variável de ambiente MUPROXY_BASE, e o byp
    /// a lê no attach — sem arquivo e sem corrida.
    ///
    /// Instância 0 = base 0 = exatamente o comportamento de hoje.
    /// </summary>
    public class InstanceAllocator
    {
        /// <summary>Espaçamento entre instâncias (evita sobreposição de faixas).</summary>
        public const int BaseStep = 100;

        public const int LoginPortBase = 44411;
        public const int CmdPortBase = 44412;
        public const int PktPortBase = 44413;

        /// <summary>Máximo de instâncias (127.0.0.1 .. 127.0.0.9).</summary>
        public const int MaxInstances = 9;

        /// <summary>Base desta instância (0, 100, 200, ...).</summary>
        public int Base { get; private set; }

        /// <summary>
        /// IP de loopback desta instância. As PORTAS do servidor (CS 44411 e a
        /// do GS) não mudam — cada instância escuta as mesmas portas no seu
        /// próprio loopback.
        /// </summary>
        public string LocalIp => $"127.0.0.{Base / BaseStep + 1}";

        /// <summary>Porta do login / server-list (a do servidor, intocável).</summary>
        public int LoginPort => LoginPortBase;

        /// <summary>Porta local do canal de comandos (44412 + base).</summary>
        public int CmdPort => CmdPortBase + Base;

        /// <summary>Porta local do canal de pacotes (44413 + base).</summary>
        public int PktPort => PktPortBase + Base;

        /// <summary>
        /// Porta local do game server: a porta REAL do GS (o CS manda ela no
        /// C116 e ela não pode ser alterada). A separação entre instâncias é
        /// pelo loopback, não pela porta.
        /// </summary>
        public int GsPort(int realPort) => realPort;

        private InstanceAllocator(int b) => Base = b;

        /// <summary>
        /// Procura a primeira base cujas 3 portas estão livres de fato
        /// (bind real). Devolve a alocação.
        /// </summary>
        public static InstanceAllocator Allocate(Action<string>? log = null)
        {
            for (int i = 0; i < MaxInstances; i++)
            {
                int b = i * BaseStep;
                var probe = new InstanceAllocator(b);
                if (probe.TryReserve(log))
                    return probe;
            }
            throw new InvalidOperationException(
                $"Nao ha espaco livre para uma nova instancia (127.0.0.1:44411 + 44412/44413 + n*{BaseStep}, ate {MaxInstances} instancias).");
        }

        /// <summary>
        /// Testa o bind real das portas desta instância; se falhar, a base é
        /// descartada. O proxy do CS/GS fica no loopback da instância
        /// (127.0.0.X) e os canais internos em 127.0.0.1 com +base.
        /// </summary>
        private bool TryReserve(Action<string>? log)
        {
            var opened = new List<TcpListener>();
            try
            {
                var localIp = IPAddress.Parse(LocalIp);
                foreach (var (addr, port) in new[]
                {
                    (localIp, LoginPort),
                    (IPAddress.Loopback, CmdPort),
                    (IPAddress.Loopback, PktPort)
                })
                {
                    var l = new TcpListener(addr, port);
                    l.Start();
                    opened.Add(l);
                }
            }
            catch (SocketException)
            {
                foreach (var l in opened) { try { l.Stop(); } catch { } }
                return false;
            }
            catch (Exception ex)
            {
                foreach (var l in opened) { try { l.Stop(); } catch { } }
                log?.Invoke($"[INSTANCE] falha ao testar portas da base {Base}: {ex.Message}");
                return false;
            }

            foreach (var l in opened) { try { l.Stop(); } catch { } }
            log?.Invoke($"[INSTANCE] base {Base} reservada (proxy {LocalIp}:{LoginPort}, cmd={CmdPort}, pkt={PktPort})");
            return true;
        }

        /// <summary>
        /// Grava a base no ambiente do processo — o main.exe filho herda na
        /// hora do CreateProcess, e o byp.dll lê no attach.
        /// </summary>
        public void ExportToEnvironment(string? sessionDir = null)
        {
            Environment.SetEnvironmentVariable("MUPROXY_BASE", Base.ToString(), EnvironmentVariableTarget.Process);
            if (!string.IsNullOrEmpty(sessionDir))
                Environment.SetEnvironmentVariable("MUPROXY_SESSION", sessionDir, EnvironmentVariableTarget.Process);
        }
    }
}
