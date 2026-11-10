// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Collections.Concurrent;
using System.Runtime.InteropServices;
using System.Text.RegularExpressions;
using xg;

namespace xgEditor.CoreCLR
{
    // One engine log line with the metadata parsed from it in the log callback.
    public sealed class LogEntry
    {
        internal LogEntry(MessageType type, string time, string message, string stackTrace, string context, long sequence)
        {
            Type = type;
            Time = time;
            Message = message;
            StackTrace = stackTrace;
            Context = context;
            Sequence = sequence;
            int newline = message.IndexOf('\n');
            FirstLine = (newline < 0 ? message : message[..newline]).TrimEnd('\r');
        }

        public MessageType Type { get; }
        // Local time of day, "HH:mm:ss.fff"; empty when the line had no timestamp prefix.
        public string Time { get; }
        public string Message { get; }
        public string FirstLine { get; }
        // Managed stack frames ("   at ...") from Debug.LogError/LogException; empty otherwise.
        public string StackTrace { get; }
        // Debug.Log context, for example "Actor 'Player'"; empty when none was given.
        public string Context { get; }
        public long Sequence { get; }
        public bool HasStackTrace => StackTrace.Length > 0;
    }

    // Editor-side store of engine log lines. It only listens: logs are posted with xg.Debug.Log* (native: xg::Log).
    // The callback can run on any thread; it parses the line and queues it, and Drain() moves entries into Entries
    // on the UI thread and raises Received there. Only lines logged after Attach() are captured.
    public static partial class EditorLog
    {
        public const int Capacity = 10000;

        private static readonly ConcurrentQueue<LogEntry> _pending = new();
        private static readonly List<LogEntry> _entries = new();
        private static readonly int[] _counts = new int[Enum.GetValues<MessageType>().Length];
        private static LogCallback? _callback;
        private static long _sequence;

        // Raised on the UI thread for every new entry. Attach custom handling here (filters, forwarding, files).
        public static event Action<LogEntry>? Received;

        public static IReadOnlyList<LogEntry> Entries => _entries;
        public static int Version { get; private set; }
        public static bool IsAttached => _callback is not null;

        public static int CountOf(MessageType type) => (uint)type < (uint)_counts.Length ? _counts[(int)type] : 0;

        public static void Attach()
        {
            if (_callback is not null)
                return;
            _callback = OnLog;
            ScriptRuntime.AddLogCallback(_callback);
        }

        public static void Detach()
        {
            if (_callback is null)
                return;
            ScriptRuntime.RemoveLogCallback(_callback);
            _callback = null;
        }

        public static bool Drain()
        {
            bool added = false;
            while (_pending.TryDequeue(out LogEntry? entry))
            {
                _entries.Add(entry);
                Count(entry.Type, 1);
                added = true;
                try
                {
                    Received?.Invoke(entry);
                }
                catch (Exception e)
                {
                    Console.Error.WriteLine($"[EditorLog] Received handler failed: {e}");
                }
            }

            if (_entries.Count > Capacity)
            {
                int excess = _entries.Count - Capacity;
                for (int i = 0; i < excess; ++i)
                    Count(_entries[i].Type, -1);
                _entries.RemoveRange(0, excess);
            }

            if (added)
                ++Version;
            return added;
        }

        public static void Clear()
        {
            _entries.Clear();
            Array.Clear(_counts);
            ++Version;
        }

        private static void Count(MessageType type, int delta)
        {
            if ((uint)type < (uint)_counts.Length)
                _counts[(int)type] += delta;
        }

        private static void OnLog(MessageType type, IntPtr message)
        {
            string line = Marshal.PtrToStringUTF8(message) ?? string.Empty;
            string body = StripPrefix(line, out string time);
            SplitStackTrace(body, out string text, out string stack);
            string context = TakeContext(ref stack);
            if (context.Length == 0)
                context = TakeContext(ref text);
            _pending.Enqueue(new LogEntry(type, time, text, stack, context, Interlocked.Increment(ref _sequence)));
        }

        // "[yyyy-MM-dd HH:mm:ss.fff] LEVEL: body" (xgCore LoggerImpl) -> body, with time = "HH:mm:ss.fff".
        private static string StripPrefix(string line, out string time)
        {
            time = string.Empty;
            if (line.Length == 0 || line[0] != '[')
                return line;
            int close = line.IndexOf("] ", StringComparison.Ordinal);
            if (close < 0)
                return line;
            string stamp = line[1..close];
            int space = stamp.LastIndexOf(' ');
            time = space < 0 ? stamp : stamp[(space + 1)..];
            int levelEnd = line.IndexOf(": ", close + 2, StringComparison.Ordinal);
            return levelEnd < 0 ? line[(close + 2)..] : line[(levelEnd + 2)..];
        }

        private static void SplitStackTrace(string body, out string text, out string stack)
        {
            int start = 0;
            while (start < body.Length)
            {
                int end = body.IndexOf('\n', start);
                int lineEnd = end < 0 ? body.Length : end;
                ReadOnlySpan<char> current = body.AsSpan(start, lineEnd - start);
                if (current.Length > 0 && char.IsWhiteSpace(current[0]) && current.TrimStart().StartsWith("at ", StringComparison.Ordinal))
                {
                    text = body[..start].TrimEnd();
                    stack = body[start..].TrimEnd();
                    return;
                }
                if (end < 0)
                    break;
                start = end + 1;
            }
            text = body.TrimEnd();
            stack = string.Empty;
        }

        // Removes a trailing " (Actor 'name')" / " (Actor <destroyed>)" added by Debug.Format and returns "Actor ...".
        private static string TakeContext(ref string text)
        {
            if (text.Length == 0)
                return string.Empty;
            Match match = ContextSuffix().Match(text);
            if (!match.Success)
                return string.Empty;
            text = text[..match.Index].TrimEnd();
            return match.Groups[1].Value;
        }

        [GeneratedRegex(@"\s\((Actor (?:'[^\r\n]*'|<destroyed>))\)\s*$")]
        private static partial Regex ContextSuffix();
    }
}
