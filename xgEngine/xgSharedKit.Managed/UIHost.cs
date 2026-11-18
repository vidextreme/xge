// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
using System.Runtime.InteropServices;

namespace xg
{
    // Runs the xgImUIKit host on the engine's main window, for a module that is loaded without the editor kernel
    // (for example the Project Hub). Call Frame from the module's Update and Stop before ScriptRuntime.Shutdown.
    public static partial class UIHost
    {
        private const string NativeLib = "xgScriptNative";
        private const uint PrototypeShellFlag = 1u << 0;
        private const uint MultiViewportFlag = 1u << 1;
        private const uint NoLayoutFileFlag = 1u << 2;
        private const uint CustomFrameFlag = 1u << 3;

        public static bool IsRunning { get; private set; }

        // saveLayout false: the window layout is neither loaded from nor saved to imgui.ini (shared with the editor).
        // customFrame: the editor's theme and window frame (title bar, window buttons, resize edges) without its shell.
        public static bool Start(bool prototypeShell = false, bool multiViewport = false, bool saveLayout = true, bool customFrame = false)
        {
            if (IsRunning || ScriptRuntime._enginePtr == 0)
                return false;
            uint flags = (prototypeShell ? PrototypeShellFlag : 0) | (multiViewport ? MultiViewportFlag : 0)
                | (saveLayout ? 0 : NoLayoutFileFlag) | (customFrame ? CustomFrameFlag : 0);
            IsRunning = XG_UIHostStart(ScriptRuntime._enginePtr, ScriptRuntime.ModuleID, flags);
            return IsRunning;
        }

        public static void Frame(float dt)
        {
            if (IsRunning)
                XG_UIHostFrame(ScriptRuntime._enginePtr, dt);
        }

        public static void Stop()
        {
            if (!IsRunning)
                return;
            IsRunning = false;
            XG_UIHostStop(ScriptRuntime._enginePtr);
        }

        [LibraryImport(NativeLib, EntryPoint = "XG_UIHostStart")]
        [return: MarshalAs(UnmanagedType.U1)]
        private static partial bool XG_UIHostStart(nint engine, uint callerModuleId, uint flags);

        [LibraryImport(NativeLib, EntryPoint = "XG_UIHostFrame")]
        private static partial void XG_UIHostFrame(nint engine, float dt);

        [LibraryImport(NativeLib, EntryPoint = "XG_UIHostStop")]
        private static partial void XG_UIHostStop(nint engine);
    }
}
