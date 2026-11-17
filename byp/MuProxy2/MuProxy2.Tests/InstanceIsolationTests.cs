using System.Net;
using System.Net.Sockets;
using WinFormsMitm.Services;
using Xunit;

namespace WinFormsMitm.Tests;

/// <summary>
/// Isolamento de instâncias do MuProxy2.
///
/// REGRA: as PORTAS do servidor são intocáveis (CS = 44411, GS = a do C116).
/// O que separa as instâncias é o LOOPBACK (127.0.0.1 / .2 / .3 ...) e, nos
/// canais internos (byp <-> MuProxy2), o deslocamento por base.
/// </summary>
public class InstanceIsolationTests
{
    [Fact]
    public void Porta_do_CS_nao_muda_entre_instancias()
    {
        // Instância 1: fica com a base 0 e passa a segurar os canais internos
        // (é o que a janela do MuProxy2 faz no start).
        var i1 = InstanceAllocator.Allocate();
        using var hook1 = new HookListenerService(new DllCommandService());
        hook1.Start(i1.CmdPort, i1.PktPort, i1.Base);

        // Instância 2: os canais internos da 1ª estão ocupados => base nova.
        var i2 = InstanceAllocator.Allocate();

        Assert.NotEqual(i1.Base, i2.Base);

        // A porta do servidor é a MESMA nas duas instâncias...
        Assert.Equal(44411, i1.LoginPort);
        Assert.Equal(44411, i2.LoginPort);

        // ...e o que muda é o loopback.
        Assert.Equal("127.0.0.1", i1.LocalIp);
        Assert.Equal("127.0.0.2", i2.LocalIp);

        // Os canais internos (nossos) é que são deslocados pela base.
        Assert.Equal(44412, i1.CmdPort);
        Assert.Equal(44413, i1.PktPort);
        Assert.Equal(i1.CmdPort + 100, i2.CmdPort);
        Assert.Equal(i1.PktPort + 100, i2.PktPort);
    }

    [Fact]
    public void Duas_instancias_escutam_o_CS_na_mesma_porta_em_loopbacks_diferentes()
    {
        var i1 = InstanceAllocator.Allocate();
        using var hook1 = new HookListenerService(new DllCommandService());
        hook1.Start(i1.CmdPort, i1.PktPort, i1.Base);

        var i2 = InstanceAllocator.Allocate();

        using var p1 = new TcpLoopbackProxy();
        using var p2 = new TcpLoopbackProxy();

        // Mesma porta do servidor, loopbacks diferentes: os dois binds coexistem.
        p1.AddListener(i1.LoginPort, "127.0.0.1", 44411, listenAddress: i1.LocalIp);
        p2.AddListener(i2.LoginPort, "127.0.0.1", 44411, listenAddress: i2.LocalIp);
        p1.Start();
        p2.Start();

        try
        {
            foreach (var ip in new[] { i1.LocalIp, i2.LocalIp })
            {
                using var client = new TcpClient();
                client.Connect(IPAddress.Parse(ip), 44411);
                Assert.True(client.Connected, $"sem listener em {ip}:44411");
            }
        }
        finally
        {
            p1.Stop();
            p2.Stop();
        }
    }
}
