using System.Text;

namespace WinFormsMitm.Tests;

/// <summary>
/// Criptografia do game server BlackRock MU - apenas Camada 1 (XOR+sub).
/// Confirmado via x32dbg: Main.dll+0x1CDF49 (xor al, 0x36) e
/// Main.dll+0x559B2 (sub al, 0x07). BlackRock NAO usa XorFilter.
/// </summary>
public static class GameServerCrypto
{
    public static byte XorKey = 0x36;
    public static byte SubKey = 0x07;

    public static byte[] Decrypt(byte[] data)
    {
        var r = new byte[data.Length];
        for (int i = 0; i < data.Length; i++)
            r[i] = unchecked((byte)((data[i] ^ XorKey) - SubKey));
        return r;
    }

    public static byte[] Encrypt(byte[] data)
    {
        var r = new byte[data.Length];
        for (int i = 0; i < data.Length; i++)
            r[i] = unchecked((byte)((data[i] + SubKey) ^ XorKey));
        return r;
    }

    public static int GetHeaderSize(byte firstByte)
    {
        return firstByte switch
        {
            0xC1 or 0xC3 => 2,
            0xC2 or 0xC4 => 3,
            _ => 0
        };
    }

    public static int GetPacketSize(byte[] data)
    {
        if (data.Length < 2) return 0;
        return data[0] switch
        {
            0xC1 or 0xC3 => data[1],
            0xC2 or 0xC4 => (data[1] << 8) | data[2],
            _ => 0
        };
    }

    public static string ToHex(byte[] data) => BitConverter.ToString(data).Replace("-", "");

    public static byte[] FromHex(string hex)
    {
        return Convert.FromHexString(hex.Replace(" ", "").Replace("-", ""));
    }
}

public class PacketLogEntry
{
    public DateTime Timestamp { get; set; }
    public string Tag { get; set; } = "";
    public string Dir { get; set; } = "";
    public int Length { get; set; }
    public string Hex { get; set; } = "";
    public byte[] Raw { get; set; } = Array.Empty<byte>();
    public byte[] Decrypted { get; set; } = Array.Empty<byte>();
    public string HeaderCategory { get; set; } = "OTHER";
}
