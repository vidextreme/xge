// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
using System.Text;

namespace xg
{
    public delegate void LogCallback(MessageType type, IntPtr message);

    public sealed partial class ScriptRuntime
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
                Subsystem.UnregisterAll();
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

        private const string NativeLib = "xgScriptNative";

        // Mirrors xg::Route (xgBase/public/xgRoute.h).
        [StructLayout(LayoutKind.Sequential)]
        private unsafe struct RouteAbi
        {
            public RouteType Type;
            public byte* Id;
            public int DepthLimit;
            public nint Predicate;
        }

        [LibraryImport(NativeLib, EntryPoint = "XG_AddLogCallbackForEngine")]
        private static partial void XG_AddLogCallbackForEngine(nint enginePtr, nint callback);

        [LibraryImport(NativeLib, EntryPoint = "XG_RemoveLogCallbackForEngine")]
        private static partial void XG_RemoveLogCallbackForEngine(nint enginePtr, nint callback);

        [LibraryImport(NativeLib, EntryPoint = "XG_SendMessageToEngine", StringMarshalling = StringMarshalling.Utf8)]
        private static partial void XG_SendMessageToEngine(nint enginePtr, string typeName, byte[] payload, int payloadSize, uint sourceID);

        [LibraryImport(NativeLib, EntryPoint = "XG_SendMessageToEngine_Route", StringMarshalling = StringMarshalling.Utf8)]
        private static partial void XG_SendMessageToEngine_Route(nint enginePtr, string typeName, byte[] payload, int payloadSize, uint sourceID, RouteAbi route);

        [LibraryImport(NativeLib, EntryPoint = "XG_GetPayloadMode")]
        private static partial PayloadMode XG_GetPayloadMode(nint enginePtr);

        public static PayloadMode PayloadMode => XG_GetPayloadMode(_enginePtr);

        [LibraryImport(NativeLib, EntryPoint = "XG_AllocateComponent", StringMarshalling = StringMarshalling.Utf8)]
        private static partial IntPtr XG_AllocateComponent(nint enginePtr, ulong actorID, string componentType, uint size);

        [LibraryImport(NativeLib, EntryPoint = "XG_RegisterSubsystemWithAlias", StringMarshalling = StringMarshalling.Utf8)]
        public static unsafe partial IntPtr RegisterSubsystemWithAlias(nint engine, uint moduleID, nint instancePtr,
                                        string subsystemName,
                                        delegate* unmanaged<nint, void> initFn,
                                        delegate* unmanaged<nint, void> shutdownFn,
                                        delegate* unmanaged<nint, float, void> updateFn);

        [LibraryImport(NativeLib, EntryPoint = "XG_UnregisterSubsystemWithAlias", StringMarshalling = StringMarshalling.Utf8)]
        [return: MarshalAs(UnmanagedType.U1)]
        public static partial bool UnregisterSubsystemWithAlias(nint engine, uint moduleID, string subsystemName);

        // ------------------------------------------------------------
        // Logging
        // ------------------------------------------------------------

        private static readonly List<LogCallback> _logCallbacks = new();

        // Registered delegates stay referenced here until Shutdown, so the GC can't collect
        // a callback the engine still holds
        public static void AddLogCallback(LogCallback cb)
        {
            if (_enginePtr == 0)
                throw new InvalidOperationException("ScriptRuntime not initialized.");

            _logCallbacks.Add(cb);
            XG_AddLogCallbackForEngine(_enginePtr, Marshal.GetFunctionPointerForDelegate(cb));
        }

        public static void RemoveLogCallback(LogCallback cb)
        {
            if (_enginePtr == 0 || !_logCallbacks.Remove(cb))
                return;

            XG_RemoveLogCallbackForEngine(_enginePtr, Marshal.GetFunctionPointerForDelegate(cb));
        }

        private static void RemoveAllLogCallbacks()
        {
            if (_enginePtr != 0)
            {
                foreach (LogCallback cb in _logCallbacks)
                    XG_RemoveLogCallbackForEngine(_enginePtr, Marshal.GetFunctionPointerForDelegate(cb));
            }
            _logCallbacks.Clear();
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

            XG_SendMessageToEngine(_enginePtr, typeName, bytes, bytes.Length, ModuleID);
        }

        public static unsafe void SendMessage(string typeName, object obj, Route route)
        {
            if (_enginePtr == 0)
                throw new InvalidOperationException("ScriptRuntime not initialized.");

            string json = System.Text.Json.JsonSerializer.Serialize(obj);
            byte[] bytes = Encoding.UTF8.GetBytes(json);

            byte[]? id = route.Id is null ? null : Encoding.UTF8.GetBytes(route.Id + "\0");
            fixed (byte* idPtr = id)
            {
                RouteAbi abi = new()
                {
                    Type = route.Type,
                    Id = idPtr,
                    DepthLimit = route.DepthLimit,
                    Predicate = route.Predicate,
                };
                XG_SendMessageToEngine_Route(_enginePtr, typeName, bytes, bytes.Length, ModuleID, abi);
            }
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
            IntPtr ptr = XG_AllocateComponent(_enginePtr, id.Value, componentType.Name, (uint)component.DataSize);
            var componentRef = new ComponentHandle<T>(ptr);
            componentRef.Bind(component);
            return componentRef;
        }
    }
}
