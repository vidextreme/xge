// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
﻿using System.Diagnostics;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.Json;
using xg;
using xg.math;

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
            TestVector3();
            TestVector2();
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


        public static void TestVector3()
        {
            Console.WriteLine("=== XGE Vec3 Interop Test ===");

            Vector3 a = new Vector3(1, 2, 3);
            Vector3 b = new Vector3(4, 5, 6);
            Vector3 min = new Vector3(0, 0, 0);
            Vector3 max = new Vector3(10, 10, 10);

            Matrix4x4 identity = new Matrix4x4
            {
                m00 = 1,
                m01 = 0,
                m02 = 0,
                m03 = 0,
                m10 = 0,
                m11 = 1,
                m12 = 0,
                m13 = 0,
                m20 = 0,
                m21 = 0,
                m22 = 1,
                m23 = 0,
                m30 = 0,
                m31 = 0,
                m32 = 0,
                m33 = 1
            };

            // Arithmetic
            Console.WriteLine($"Add: {a.Add(b).x}, {a.Add(b).y}, {a.Add(b).z}");
            Console.WriteLine($"Sub: {a.Sub(b).x}, {a.Sub(b).y}, {a.Sub(b).z}");
            Console.WriteLine($"Mul: {a.Mul(2).x}, {a.Mul(2).y}, {a.Mul(2).z}");
            Console.WriteLine($"Div: {a.Div(2).x}, {a.Div(2).y}, {a.Div(2).z}");

            // Vector math
            Console.WriteLine($"Length(a): {a.Length()}");
            Console.WriteLine($"LengthSquared(a): {a.LengthSquared()}");
            Vector3 norm = a.Normalized();
            Console.WriteLine($"Normalize(a): {norm.x}, {norm.y}, {norm.z}");

            Console.WriteLine($"Dot(a,b): {a.Dot(b)}");
            Vector3 cross = a.Cross(b);
            Console.WriteLine($"Cross(a,b): {cross.x}, {cross.y}, {cross.z}");

            // Distance
            Console.WriteLine($"Distance(a,b): {a.Distance(b)}");
            Console.WriteLine($"DistanceSquared(a,b): {a.DistanceSquared(b)}");

            // Lerp / Min / Max / Clamp
            Vector3 lerp = a.Lerp(b, 0.5f);
            Console.WriteLine($"Lerp(a,b,0.5): {lerp.x}, {lerp.y}, {lerp.z}");

            Vector3 minv = a.Min(b);
            Console.WriteLine($"Min(a,b): {minv.x}, {minv.y}, {minv.z}");

            Vector3 maxv = a.Max(b);
            Console.WriteLine($"Max(a,b): {maxv.x}, {maxv.y}, {maxv.z}");

            Vector3 clamped = a.Clamp(min, max);
            Console.WriteLine($"Clamp(a,0..10): {clamped.x}, {clamped.y}, {clamped.z}");

            // Transform
            Vector3 transformed = a.Transform(identity);
            Console.WriteLine($"Transform(a, identity): {transformed.x}, {transformed.y}, {transformed.z}");

            Vector3 dirTransformed = a.TransformDirection(identity);
            Console.WriteLine($"TransformDirection(a, identity): {dirTransformed.x}, {dirTransformed.y}, {dirTransformed.z}");

            // Utility
            Console.WriteLine($"IsZero(a): {a.IsZero()}");
            Console.WriteLine($"IsEqual(a,b): {a.IsEqual(b)}");
            Console.WriteLine($"IsEqual(a,a): {a.IsEqual(a)}");

            Console.WriteLine("=== XGE Vec3 Interop Test Complete ===");
        }

        public static void TestVector2()
        {
            Console.WriteLine("=== XGE Vec2 Interop Test ===");

            Vector2 a = new Vector2(3f, 4f);
            Vector2 b = new Vector2(1f, 2f);
            Vector2 min = new Vector2(0f, 0f);
            Vector2 max = new Vector2(10f, 10f);

            // Arithmetic
            Vector2 add = a.Add(b);
            Console.WriteLine($"Add(a,b): {add.x}, {add.y}");

            Vector2 sub = a.Sub(b);
            Console.WriteLine($"Sub(a,b): {sub.x}, {sub.y}");

            Vector2 mul = a.Mul(2f);
            Console.WriteLine($"Mul(a,2): {mul.x}, {mul.y}");

            Vector2 div = a.Div(2f);
            Console.WriteLine($"Div(a,2): {div.x}, {div.y}");

            // Vector math
            Console.WriteLine($"Length(a): {a.Length()}");
            Console.WriteLine($"LengthSquared(a): {a.LengthSquared()}");

            Vector2 norm = a.Normalized();
            Console.WriteLine($"Normalize(a): {norm.x}, {norm.y}");

            Console.WriteLine($"Dot(a,b): {a.Dot(b)}");

            // Distance
            Console.WriteLine($"Distance(a,b): {a.Distance(b)}");
            Console.WriteLine($"DistanceSquared(a,b): {a.DistanceSquared(b)}");

            // Lerp / Min / Max / Clamp
            Vector2 lerp = a.Lerp(b, 0.5f);
            Console.WriteLine($"Lerp(a,b,0.5): {lerp.x}, {lerp.y}");

            Vector2 minv = a.Min(b);
            Console.WriteLine($"Min(a,b): {minv.x}, {minv.y}");

            Vector2 maxv = a.Max(b);
            Console.WriteLine($"Max(a,b): {maxv.x}, {maxv.y}");

            Vector2 clamped = a.Clamp(min, max);
            Console.WriteLine($"Clamp(a,0..10): {clamped.x}, {clamped.y}");

            // Utility
            Console.WriteLine($"IsZero(a): {a.IsZero()}");
            Console.WriteLine($"IsEqual(a,b): {a.IsEqual(b)}");
            Console.WriteLine($"IsEqual(a,a): {a.IsEqual(a)}");

            Console.WriteLine("=== XGE Vec2 Interop Test Complete ===");
        }
    }
}
