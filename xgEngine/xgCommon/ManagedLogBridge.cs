using System;
using System.Runtime.InteropServices;

namespace xg
{
    public static class ManagedLogBridge
    {
        // Native callback signature
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void ManagedLogCallback(MessageType type, IntPtr message);

        // Native imports
        [DllImport("xgEngine", CallingConvention = CallingConvention.Cdecl)]
        private static extern void AddLogCallback(ManagedLogCallback callback);

        [DllImport("xgEngine", CallingConvention = CallingConvention.Cdecl)]
        private static extern void RemoveLogCallback(ManagedLogCallback callback);

        [DllImport("xgEngine", CallingConvention = CallingConvention.Cdecl)]
        public static extern void SetMinimumLogLevel(MessageType type);

        [DllImport("xgEngine", CallingConvention = CallingConvention.Cdecl)]
        public static extern void Log(MessageType type, string message);

        private static ManagedLogCallback? _callback;
        private static GCHandle _callbackHandle;

        public static event Action<MessageType, string>? OnLog;

        public static void Initialize()
        {
            if (_callback != null)
                return;

            _callback = OnNativeLog;
            _callbackHandle = GCHandle.Alloc(_callback);

            AddLogCallback(_callback);
        }

        private static void OnNativeLog(MessageType type, IntPtr msgPtr)
        {
            string msg = Marshal.PtrToStringUTF8(msgPtr) ?? string.Empty;
            OnLog?.Invoke(type, msg);
        }

        public static void Shutdown()
        {
            if (_callback != null)
            {
                RemoveLogCallback(_callback);
                _callback = null;
            }

            if (_callbackHandle.IsAllocated)
                _callbackHandle.Free();
        }
    }
}
