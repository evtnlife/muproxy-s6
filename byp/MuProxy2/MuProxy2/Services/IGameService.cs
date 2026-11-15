namespace WinFormsMitm.Services
{
    /// <summary>
    /// Interface base para todos os serviços do jogo.
    /// Cada serviço registra-se no ServiceHub e recebe notificações de ciclo de vida.
    /// </summary>
    public interface IGameService
    {
        /// <summary>Nome único do serviço para identificação nos logs.</summary>
        string Name { get; }

        /// <summary>Chamado quando o byp.dll conecta via TCP 44412.</summary>
        void OnDllConnected();

        /// <summary>Chamado quando o byp.dll desconecta.</summary>
        void OnDllDisconnected();

        /// <summary>Chamado durante shutdown do proxy.</summary>
        void Shutdown();
    }
}
