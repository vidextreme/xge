// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
using System.Runtime.InteropServices;

namespace xg
{
    // The startup splash shown while xge loads its modules. Calls do nothing once the main window is up, so modules
    // can report from Init without checking. Call only from the main thread.
    public static partial class Startup
    {
        private const string NativeLib = "xgScriptNative";

        // Indeterminate progress: the bar shows a moving segment instead of a fill.
        public const float Indeterminate = -1.0f;

        // Keeps the bar as it is (only the text changes).
        public const float KeepProgress = float.NaN;

        // step null keeps the current step; detail null keeps the current line and "" clears it.
        public static void Report(string? step, string? detail, float progress)
        {
            if (ScriptRuntime._enginePtr != 0)
                XG_StartupReport(ScriptRuntime._enginePtr, step, detail, progress);
        }

        // Keeps the splash responsive during long blocking work; call it every few tens of milliseconds.
        public static void Pump()
        {
            if (ScriptRuntime._enginePtr != 0)
                XG_StartupPump(ScriptRuntime._enginePtr);
        }

        [LibraryImport(NativeLib, EntryPoint = "XG_StartupReport", StringMarshalling = StringMarshalling.Utf8)]
        private static partial void XG_StartupReport(nint enginePtr, string? step, string? detail, float progress);

        [LibraryImport(NativeLib, EntryPoint = "XG_StartupPump")]
        private static partial void XG_StartupPump(nint enginePtr);
    }
}
