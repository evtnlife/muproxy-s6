namespace WinFormsMitm.Events
{
    public abstract record MuEvent(DateTime Timestamp);
    public record LogEvent(DateTime Timestamp, string Message) : MuEvent(Timestamp);
}
