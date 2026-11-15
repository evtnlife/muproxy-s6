using System.Text;
using System.Text.RegularExpressions;
using System.Globalization;

namespace WinFormsMitm.Tests;

public static class CryptoBrute
{
    public static byte[] Transform(byte[] data, byte xorKey, byte subKey, bool addFirst)
    {
        var r = new byte[data.Length];
        for (int i = 0; i < data.Length; i++)
        {
            if (addFirst)
                r[i] = unchecked((byte)((data[i] + subKey) ^ xorKey));
            else
                r[i] = unchecked((byte)((data[i] ^ xorKey) - subKey));
        }
        return r;
    }

    public static string Header(byte[] data) => data.Length > 0 ? $"{data[0]:X2}" : "";
}

public class PacketBruteEntry
{
    public string OriginalHex { get; set; } = "";
    public byte[] Raw { get; set; } = Array.Empty<byte>();
    public string Dir { get; set; } = "";
}
