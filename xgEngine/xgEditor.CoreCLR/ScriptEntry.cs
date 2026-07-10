// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
﻿using System.Diagnostics;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.Json;
using xg;
namespace xgEditor.CoreCLR
{
    public static unsafe class ScriptEntry
    {
        [UnmanagedCallersOnly]
        public static int Init(nint enginePtr, uint moduleID)
        {
            ScriptRuntime.Initialize(enginePtr, moduleID);

            // Example: register a managed log callback
            ScriptRuntime.AddLogCallback(OnLog);
            Console.WriteLine("wow");
            return 1;
        }

        [UnmanagedCallersOnly]
        public static void Update(float dt) 
        {
            
        }

        [UnmanagedCallersOnly]
        public static void Shutdown() 
        {

        }

        private static void OnLog(MessageType type, IntPtr msgPtr)
        {
            string msg = Marshal.PtrToStringAnsi(msgPtr) ?? string.Empty;
           // Console.WriteLine($"[{type}] {msg} waasdadsds");
        }

        public class TestType
        {
            public int Health { get; set; }
            public string Name { get; set; }
        }


        [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
        public static void OnMessage(nint typeName, nint payload, int payloadSize)
        {
            string typeNameStr = Marshal.PtrToStringAnsi(typeName) ?? string.Empty;

            if (payload == 0 || payloadSize == 0)
                return;

            if (ScriptRuntime.PayloadMode == PayloadMode.BINARY)
            {
                // Build interop struct
                var msg = new ScriptMessageInterop
                {
                    Payload = (byte*)payload,
                    PayloadSize = payloadSize,
                    TypeName = typeNameStr,
                    SourceID = 0 // managed
                };

                var parser = new ScriptMessageParser(msg);

                // Example decode for TestType
                if (typeNameStr == "TestType")
                {
                    int health = parser.ReadPrimitive<int>();
                    string name = parser.ReadString();

                    var obj = new TestType { Health = health, Name = name };

                    ScriptRuntime.DispatchMessage(typeNameStr, obj);
                    return;
                }

                Console.WriteLine($"[Managed] Unknown binary type: {typeNameStr}");
                return;
            }
            else
            {
                // JSON path (your existing logic)
                byte[] data = new byte[payloadSize];
                Marshal.Copy(payload, data, 0, payloadSize);

                string json = Encoding.UTF8.GetString(data);
                Console.WriteLine($"[Managed] Raw JSON: {json}");

                if (typeNameStr == "TestType")
                {
                    var obj2 = JsonSerializer.Deserialize<TestType>(json);
                    ScriptRuntime.DispatchMessage(typeNameStr, obj2);
                }
                else
                {
                    Console.WriteLine($"Unknown type {typeNameStr}: {json}");
                }
            }
        }

    }
}
