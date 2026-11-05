// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
using System.Text;

namespace xg
{
    public delegate void LogCallback(MessageType type, IntPtr message);

    public sealed class ScriptRuntime
    {
        public static uint ModuleID { get; private set; }
        internal static nint _enginePtr;
        public static void Initialize(nint ptr, uint moduleID)
        {
            _enginePtr = ptr;
            ModuleID = moduleID;

            // Catch managed/native math struct size drift before it corrupts memory.
            // Not rethrown: Initialize runs inside an UnmanagedCallersOnly entry point.
            try
            {
                xg.math.MathLayout.Validate();
            }
            catch (Exception e)
            {
                Debug.LogError($"[xgSharedKit] {e.Message}");
            }

            try
            {
                ActorRegistry.Attach(ptr, moduleID);
            }
            catch (Exception e)
            {
                Debug.LogError($"[xgSharedKit] Actor update registration failed: {e.Message}");
            }
        }

        public static void Shutdown()
        {
            try
            {
                RemoveAllLogCallbacks();
            }
            catch (Exception e)
            {
                Debug.LogException(e);
            }

            try
            {
                ActorRegistry.Detach(_enginePtr);
            }
            catch (Exception e)
            {
                Debug.LogException(e);
            }
        }

        // ------------------------------------------------------------
        // Native interop
        // ------------------------------------------------------------

        [DllImport("xgScriptNative", CallingConvention = CallingConvention.Cdecl,
            EntryPoint = "XGAddLogCallbackForEngine")]
        private static extern void XGAddLogCallbackForEngine(
            nint enginePtr,
            LogCallback callback);

        [DllImport("xgScriptNative", CallingConvention = CallingConvention.Cdecl,
            EntryPoint = "XGRemoveLogCallbackForEngine")]
        private static extern void XGRemoveLogCallbackForEngine(
            nint enginePtr,
            LogCallback callback);

        [DllImport("xgScriptNative", CallingConvention = CallingConvention.Cdecl,
            EntryPoint = "XGSendMessageToEngine")]
        private static extern void XGSendMessageToEngine(
            nint enginePtr,
            string typeName,
            byte[] payload,
            int payloadSize,
            uint sourceID);

        [DllImport("xgScriptNative", CallingConvention = CallingConvention.Cdecl,
            EntryPoint = "XGSendMessageToEngine_Route")]
        private static extern void XGSendMessageToEngine_Route(
            nint enginePtr,
            string typeName,
            byte[] payload,
            int payloadSize,
            uint sourceID,
            Route route);

        [DllImport("xgScriptNative", CallingConvention = CallingConvention.Cdecl)]
        private static extern PayloadMode XGGetPayloadMode(nint enginePtr);

        public static PayloadMode PayloadMode => XGGetPayloadMode(_enginePtr);

        [DllImport("xgScriptNative", CallingConvention = CallingConvention.Cdecl,
         EntryPoint = "XGAllocateComponent")]
        private static extern IntPtr XGAllocateComponent(nint enginePtr, ulong actorID, string componentType, uint size);

        [DllImport("xgScriptNative", EntryPoint = "XGRegiterSystemWithAlias")]
        public static extern unsafe IntPtr RegisterSystemWithAlias(nint engine, uint moduleID, nint instancePtr,
                                        string systemName,
                                        delegate* unmanaged<nint, void> initFn,
                                        delegate* unmanaged<nint, void> shutdownFn,
                                        delegate* unmanaged<nint, float, void> updateFn);

        // ------------------------------------------------------------
        // Logging
        // ------------------------------------------------------------

        private static readonly List<LogCallback> s_logCallbacks = new();

        // Registered delegates stay referenced here until Shutdown, so the GC can't collect
        // a callback the engine still holds
        public static void AddLogCallback(LogCallback cb)
        {
            if (_enginePtr == 0)
                throw new InvalidOperationException("ScriptRuntime not initialized.");

            s_logCallbacks.Add(cb);
            XGAddLogCallbackForEngine(_enginePtr, cb);
        }

        public static void RemoveLogCallback(LogCallback cb)
        {
            if (_enginePtr == 0 || !s_logCallbacks.Remove(cb))
                return;

            XGRemoveLogCallbackForEngine(_enginePtr, cb);
        }

        private static void RemoveAllLogCallbacks()
        {
            if (_enginePtr != 0)
            {
                foreach (LogCallback cb in s_logCallbacks)
                    XGRemoveLogCallbackForEngine(_enginePtr, cb);
            }
            s_logCallbacks.Clear();
        }

        // ------------------------------------------------------------
        // Managed → Native messaging
        // ------------------------------------------------------------

        public static void SendMessage(string typeName, object obj)
        {
            if (_enginePtr == 0)
                throw new InvalidOperationException("ScriptRuntime not initialized.");

            string json = System.Text.Json.JsonSerializer.Serialize(obj);
            byte[] bytes = Encoding.UTF8.GetBytes(json);

            XGSendMessageToEngine(_enginePtr, typeName, bytes, bytes.Length, ModuleID);
        }

        public static void SendMessage(string typeName, object obj, Route route)
        {
            if (_enginePtr == 0)
                throw new InvalidOperationException("ScriptRuntime not initialized.");

            string json = System.Text.Json.JsonSerializer.Serialize(obj);
            byte[] bytes = Encoding.UTF8.GetBytes(json);

            XGSendMessageToEngine_Route(_enginePtr, typeName, bytes, bytes.Length, ModuleID, route);
        }

        // ------------------------------------------------------------
        // Native → Managed dispatch
        // ------------------------------------------------------------

        public static void DispatchMessage(string typeName, object obj)
        {
            Console.WriteLine($"[Managed] Message received: {typeName}");
        }

        public static unsafe ComponentHandle<T> AllocateComponent<T>(ActorID id)
            where T : ComponentOLD, new()
        {
            var componentType = typeof(T);
            T component = new();
            IntPtr ptr = XGAllocateComponent(_enginePtr, id.Value, componentType.Name, (uint)component.DataSize);
            var componentRef = new ComponentHandle<T>(ptr);
            componentRef.Bind(component);
            return componentRef;
        }
    }
}
