using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;


namespace xg
{
    public delegate void LogCallback(MessageType type, IntPtr message);
    public sealed class ScriptEngine
    {
        private static nint _nativePtr;

        public static void Initialize(nint ptr)
        {
            _nativePtr = ptr;
        }

        [DllImport("xgScriptNative", CallingConvention = CallingConvention.Cdecl,
          EntryPoint = "XGAddLogCallbackForEngine")]
        private static extern void XGAddLogCallbackForEngine(
          nint enginePtr,
          LogCallback callback);

        public static void AddLogCallback(LogCallback cb)
        {
            if (_nativePtr == 0)
                throw new InvalidOperationException("ScriptEngine not initialized.");

            XGAddLogCallbackForEngine(_nativePtr, cb);
        }
    }

}
