// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
﻿using System;
using System.Runtime.InteropServices;
using System.Text;

namespace xg
{
    public delegate void LogCallback(MessageType type, IntPtr message);

    public sealed class ScriptRuntime
    {
        public static uint ModuleID { get; private set; }
        private static nint _enginePtr;        
        public static void Initialize(nint ptr, uint moduleID)
        {
            _enginePtr = ptr;
            ModuleID = moduleID;
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


        // ------------------------------------------------------------
        // Logging
        // ------------------------------------------------------------

        public static void AddLogCallback(LogCallback cb)
        {
            if (_enginePtr == 0)
                throw new InvalidOperationException("ScriptRuntime not initialized.");

            XGAddLogCallbackForEngine(_enginePtr, cb);
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
    }
}
