using System.Reflection;

namespace WinFormsMitm.Messages
{
    public static class MessageRegistry
    {
        private static readonly Dictionary<string, Func<string, object>> _parsers = new();

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
