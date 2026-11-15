namespace WinFormsMitm.Messages
{
    [AttributeUsage(AttributeTargets.Class, Inherited = false)]
    public class MessageTagAttribute : Attribute
    {
        public string Tag { get; }
        public MessageTagAttribute(string tag) => Tag = tag;
    }
}
