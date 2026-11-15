using System.Reflection;

namespace WinFormsMitm.Protocol
{
    /// <summary>
    /// Registry de mensagens tipadas. No startup, escaneia o assembly por classes
    /// com [MessageTag] e registra o parser de cada uma. O(1) no dispatch.
    /// </summary>
    public static class MessageRegistry
    {
        private static readonly Dictionary<string, Func<string, object>> _parsers = new(StringComparer.Ordinal);

        static MessageRegistry()
        {
            foreach (var type in Assembly.GetExecutingAssembly().GetTypes())
            {
                var attr = type.GetCustomAttribute<MessageTagAttribute>();
                if (attr == null) continue;

                var method = type.GetMethod("Parse", BindingFlags.Public | BindingFlags.Static);
                if (method == null || method.ReturnType != type) continue;

                var parser = (Func<string, object>)Delegate.CreateDelegate(
                    typeof(Func<string, object>), method);

                _parsers[attr.Tag] = parser;
            }
        }

        /// <summary>
        /// Parse uma linha do protocolo: "TAG|payload".
        /// Retorna a instância da classe correspondente ou null se desconhecido.
        /// </summary>
        public static object? Parse(string line)
        {
            int sep = line.IndexOf('|');
            if (sep < 0) return null;

            string tag = line.Substring(0, sep);
            string payload = line.Substring(sep + 1);

            return _parsers.TryGetValue(tag, out var parser) ? parser(payload) : null;
        }
    }
}
