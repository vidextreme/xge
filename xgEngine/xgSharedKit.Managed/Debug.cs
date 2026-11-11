// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Diagnostics;
using System.Runtime.CompilerServices;

namespace xg
{
    public static partial class Debug
    {
        [MethodImpl(MethodImplOptions.NoInlining)]
        public static void Log(object? message) => Send(MessageType.Info, Format(message, null), false);

        [MethodImpl(MethodImplOptions.NoInlining)]
        public static void Log(object? message, Actor? context) => Send(MessageType.Info, Format(message, context), false);

        [MethodImpl(MethodImplOptions.NoInlining)]
        public static void LogFormat(string format, params object?[] args) => Send(MessageType.Info, SafeFormat(format, args), false);

        [MethodImpl(MethodImplOptions.NoInlining)]
        public static void LogSuccess(object? message) => Send(MessageType.Success, Format(message, null), false);

        [MethodImpl(MethodImplOptions.NoInlining)]
        public static void LogSuccess(object? message, Actor? context) => Send(MessageType.Success, Format(message, context), false);

        [MethodImpl(MethodImplOptions.NoInlining)]
        public static void LogWarning(object? message) => Send(MessageType.Warning, Format(message, null), false);

        [MethodImpl(MethodImplOptions.NoInlining)]
        public static void LogWarning(object? message, Actor? context) => Send(MessageType.Warning, Format(message, context), false);

        [MethodImpl(MethodImplOptions.NoInlining)]
        public static void LogWarningFormat(string format, params object?[] args) => Send(MessageType.Warning, SafeFormat(format, args), false);

        [MethodImpl(MethodImplOptions.NoInlining)]
        public static void LogError(object? message) => Send(MessageType.Error, Format(message, null), true);

        [MethodImpl(MethodImplOptions.NoInlining)]
        public static void LogError(object? message, Actor? context) => Send(MessageType.Error, Format(message, context), true);

        [MethodImpl(MethodImplOptions.NoInlining)]
        public static void LogErrorFormat(string format, params object?[] args) => Send(MessageType.Error, SafeFormat(format, args), true);

        [MethodImpl(MethodImplOptions.NoInlining)]
        public static void LogException(Exception exception, Actor? context = null) =>
            Send(MessageType.Error, Format(exception, context), false);

        [Conditional("DEBUG")]
        [MethodImpl(MethodImplOptions.NoInlining)]
        public static void Assert(bool condition, object? message = null)
        {
            if (!condition)
                Send(MessageType.Error, message is null ? "Assertion failed" : "Assertion failed: " + ToText(message), true);
        }

        private static string ToText(object? message)
        {
            try
            {
                return message?.ToString() ?? "Null";
            }
            catch (Exception e)
            {
                return $"<{message!.GetType().Name}.ToString() threw {e.GetType().Name}>";
            }
        }

        private static string Format(object? message, Actor? context)
        {
            string text = ToText(message);
            if (context is null)
                return text;
            return context.IsValid ? $"{text} (Actor '{context.Name}')" : $"{text} (Actor <destroyed>)";
        }

        private static string SafeFormat(string format, object?[] args)
        {
            try
            {
                return string.Format(format, args);
            }
            catch (FormatException)
            {
                return format;
            }
        }

        [MethodImpl(MethodImplOptions.NoInlining)]
        private static void Send(MessageType type, string text, bool withStackTrace)
        {
            if (withStackTrace)
            {
                try
                {
                    text += Environment.NewLine + new StackTrace(2, true).ToString().TrimEnd();
                }
                catch
                {
                }
            }

            nint engine = ScriptRuntime._enginePtr;
            if (engine != 0)
            {
                try
                {
                    XG_LogForEngine(engine, (int)type, text);
                    return;
                }
                catch
                {
                }
            }

            WriteFallback(type, text);
        }

        private static void WriteFallback(MessageType type, string text)
        {
            try
            {
                string line = $"[xg] {LevelName(type)}: {text}";
                if (type >= MessageType.Warning)
                    Console.Error.WriteLine(line);
                else
                    Console.Out.WriteLine(line);
            }
            catch
            {
            }
        }

        private static string LevelName(MessageType type) => type switch
        {
            MessageType.Info => "INFO",
            MessageType.Success => "SUCC",
            MessageType.Warning => "WARN",
            MessageType.Critical => "CRIT",
            _ => "ERR",
        };

        [LibraryImport("xgScriptNative", EntryPoint = "XG_LogForEngine", StringMarshalling = StringMarshalling.Utf8)]
        private static partial void XG_LogForEngine(nint engine, int type, string message);
    }
}
