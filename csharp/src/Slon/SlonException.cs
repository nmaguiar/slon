namespace Slon;

public class SlonException : Exception
{
    public SlonException(string message) : base(message)
    {
    }

    public SlonException(string message, Exception innerException) : base(message, innerException)
    {
    }
}
