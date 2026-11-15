using System.Text;
using Xunit;
using Xunit.Abstractions;

namespace WinFormsMitm.Tests;

/// <summary>
/// Teste com dados REAIS capturados do proxy em 2026-08-05 17:29.
/// Valida a decifracao completa (Camada 1 + Camada 2) contra pacotes do BlackRock MU.
/// </summary>
public class RealDataDecryptionTests
{
    private readonly ITestOutputHelper _output;

    public RealDataDecryptionTests(ITestOutputHelper output)
    {
        _output = output;
    }

    [Fact]
    public void Decrypt_Real_Packets_From_Proxy_Output()
    {
        // Pacotes reais capturados do proxy 55902 em 2026-08-05 17:29
        var packets = new (string dir, string hex)[]
        {
            // Primeiro pacote do game server (handshake)
            ("S->C", "FE25CE313E0C310F0E0D0131"),
            // Resposta do cliente (login)
            ("C->S", "FCBBA1AE0F90BFB181C51EE3346A1238C8D6B8CCE3B21605A8A468DF3B861C8ADD0017B91A15B7E582FC621D98AF1CF35773158E11163A2C1BA05861C298BB38FA69051667EB39B2B9091837D16C5B3FDE1DADACB711B6C2C7D0478A4BA9EB2E6E6E8A445312F863EAA68DEE7E02281FCBFA0E63EE72AF93D567B0BF894E7DD1FDECAB39230A"),
            // Ping do servidor
            ("S->C", "FE3ACE3E3E"),
            // Pong do cliente
            ("C->S", "FE3ACCCAC3"),
            // Outro pacote do cliente
            ("C->S", "FC29273E032F784C4C821172393464120A6C83815E5EC1E3"),
            // Keep-alive
            ("C->S", "FE3DCC22"),
            // Dados do servidor (character list?)
            ("S->C", "FF3151CC313E313E3131313134F12D3A31313131317A4D5A4F435E456846B731603131313A3B0D0C0C3EBD2131313191F131213130313131313131313131313131313131313131313030303031313131313131313131313131313131313131"),
            // Mais ping/pong
            ("S->C", "FE3AD3313E"),
            ("C->S", "FE23CC297A2F8F5CF8AA099C858E"),
            ("S->C", "FE20CC2A7A4D5A4F435E456846B731"),
            ("C->S", "FE23CC236C3D9D4AEECC038AF798"),
            // Pacote C3 (FC) do cliente
            ("C->S", "FC225C2E6256135BCF5041703A"),
            // Keep-alive
            ("C->S", "FE3DCC37"),
            // Mais dados
            ("C->S", "FE39CC3514F8329E"),
            ("S->C", "FE27CCC83C3114311A31"),
            ("S->C", "FE3DCCCE"),
            ("C->S", "FE3A29D8BA"),
            ("C->S", "FE39EDE61414E560"),
            ("S->C", "FE39ED0C31DAF811"),
            ("C->S", "FE38EDE31310F9"),
            ("C->S", "FC2921401FA429925F35421912ADC35D23227E1BD1A6EAC4"),
            ("S->C", "FE39ED0C31DAFB11"),
            ("C->S", "FE38EDE32E21C8"),
            ("C->S", "FE38EDE02E21F8"),
            ("S->C", "FE39ED0C31DBFA11"),
            ("S->C", "FE39ED0C31DBFD11"),
            ("C->S", "FE38EDE02F2CCD"),
            ("S->C", "FE39ED0C31D8FC11"),
            ("S->C", "FE3D202B"),
            ("C->S", "FE27A34FC696F624E332"),
            ("S->C", "FC2968F31F4F23C88E6F42E6359DB2B5AA539E51E7C985A1"),
            ("C->S", "FE3DCC10"),
            ("C->S", "FE3A29DBBD"),
            ("C->S", "FE39CC35DAFB3C91"),
            ("S->C", "FE3DCCCE"),
            ("S->C", "FE3D203B"),
            ("C->S", "FE39CC35DAFB3C91"),
            ("S->C", "FE3DCCCE"),
            ("S->C", "FE3D203D"),
            ("C->S", "FE39CC35DAFB3C91"),
            ("S->C", "FE3DCCCE"),
            // Pacotes com texto ASCII visivel (chat/notice)
            ("S->C", "FE7022313131313131313131316D595A116F5A5D117D4F5E5840431166434B5E4C464043114C4D5E4F4D4C114643110F114246434A4D5A194C061E31313131"),
            ("S->C", "FE7222313131313131313131316D595A11635A4811565A5E4F1166434B5E4C464043114C4D5E4F4D4C114643110F114246434A4D5A194C061E31313131"),
            ("C->S", "FC2915133D4697362F5CCD50476F5F519F391BCBF2619193"),
        };

        var sb = new StringBuilder();
        sb.AppendLine("# Validacao de Decifracao com Dados Reais do Proxy");
        sb.AppendLine($"# Algoritmo: Camada1=(c^0x{XorKey:X2})-0x{SubKey:X2} + Camada2=XorFilter[32]");
        sb.AppendLine();

        int ok = 0, fail = 0;

        foreach (var (dir, hex) in packets)
        {
            var raw = GameServerCrypto.FromHex(hex);
            var dec = GameServerCrypto.Decrypt(raw);
            var header = dec.Length > 0 ? dec[0] : (byte)0;
            var isMu = header is 0xC1 or 0xC2 or 0xC3 or 0xC4;
            var pktSize = isMu ? GameServerCrypto.GetPacketSize(dec) : 0;
            var sizeOk = isMu && pktSize > 0 && pktSize <= dec.Length;

            if (isMu && sizeOk)
            {
                ok++;
                if (ok <= 10) // mostra so os primeiros 10 OK
                {
                    sb.AppendLine($"[{dir}] OK header=0x{header:X2} size={pktSize} len={dec.Length}");
                    sb.AppendLine($"  raw: {hex}");
                    sb.AppendLine($"  dec: {GameServerCrypto.ToHex(dec)}");
                    sb.AppendLine();
                }
            }
            else
            {
                fail++;
                sb.AppendLine($"[{dir}] FALHA header=0x{header:X2} pktSize={pktSize} len={dec.Length}");
                sb.AppendLine($"  raw: {hex}");
                sb.AppendLine($"  dec: {GameServerCrypto.ToHex(dec)}");
                sb.AppendLine();
            }
        }

        sb.AppendLine($"Resultado: {ok} OK, {fail} FALHAS de {packets.Length} pacotes");

        _output.WriteLine(sb.ToString());

        Assert.True(fail == 0, $"{fail} pacotes falharam na decifracao");
    }

    private static byte XorKey => GameServerCrypto.XorKey;
    private static byte SubKey => GameServerCrypto.SubKey;
}
