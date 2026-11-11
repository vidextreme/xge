// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
﻿using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.Json;
using xg;
using xg.math;

namespace xgEditor.CoreCLR
{
    // Test hook: a managed subsystem registered through xg.Subsystem. Logs its lifecycle.
    internal sealed class SampleSubsystem : ISubsystem
    {
        public string Name { get; set; } = "sample";

        public void Initialize() => Debug.Log($"[Debug] Subsystem {Name}: initialized");
        public void Update(float dt) { }
        public void Shutdown() => Debug.Log($"[Debug] Subsystem {Name}: shutdown");
    }

    public static unsafe class ScriptEntry
    {
        [UnmanagedCallersOnly]
        public static int Init(nint enginePtr, uint moduleID)
        {
            ScriptRuntime.Initialize(enginePtr, moduleID);
            EditorLog.Attach();

            // Example: register a managed log callback
            ScriptRuntime.AddLogCallback(OnLog);
            Console.WriteLine("wow");
            TestVector3();
            TestVector2();
            TestQuaternion();
            TestMatrix4x4();
            
            Subsystem.Register<SampleSubsystem>("managed.system").Name = "managed.system";
            Subsystem.Register(new SampleSubsystem { Name = "managed.temp" }, "managed.temp");
            Debug.Log($"[Debug] Subsystem managed.temp unregistered: {Subsystem.Unregister("managed.temp")}, registered: [{string.Join(", ", Subsystem.Names)}]");
            Subsystem.Register<SampleSubsystem>("managed.system");

            var actor = xg.Actor.Create("walaweh!");
            Console.WriteLine(actor.Name);
            actor.Name = "REALLY AWWESOMEH!!!";
            Console.WriteLine(actor.Name);

            _player = actor.AddComponent<PlayerComponent>();
            _player.Data.Stats.Health = 100;

            _temporary = xg.Actor.Create("temporary");
            _temporaryEnemy = _temporary.AddComponent<EnemyComponent>();

            Debug.Log("[Debug] editor module initialized", actor);
            Debug.LogSuccess("[Debug] success level");
            Debug.LogWarningFormat("[Debug] {0} actor(s) registered, 100% {1}", ActorRegistry.Count, "ok");
            Debug.LogError("[Debug] sample error with caller stack");
            try
            {
                throw new InvalidOperationException("sample exception");
            }
            catch (Exception e)
            {
                Debug.LogException(e, _temporary);
            }
            Debug.Assert(ActorRegistry.Count == 2, "expected two actors");
            Debug.Log(null);

            xg.Actor body = xg.Actor.Create("Body");
            xg.Actor weapon = xg.Actor.Create("Weapon");
            xg.Actor muzzle = xg.Actor.Create("Muzzle");
            body.Parent = actor;
            weapon.Parent = actor;
            muzzle.Parent = weapon;
            List<ActorID> children = new();
            ActorTree.GetChildren(actor.ID, children);
            Debug.Log($"[Debug] Hierarchy sample: {actor.Name} children [{string.Join(", ", children.Select(ActorTree.GetName))}], Muzzle parent {muzzle.Parent?.Name}");

            HierarchyPanel hierarchy = xg.imui.Panel.Open<HierarchyPanel>();
            if (Environment.GetEnvironmentVariable("XG_UI_TEST_FOCUS") == "hierarchy")
            {
                hierarchy.Focus();
                hierarchy.ExpandAll();
                hierarchy.SelectByName("Weapon");
            }

            ConsolePanel console = xg.imui.Panel.Open<ConsolePanel>();
            if (Environment.GetEnvironmentVariable("XG_UI_TEST_FOCUS") == "console")
            {
                console.Focus();
                console.SelectLatestError();
            }

            _testWindow = xg.imui.Panel.Open<ManagedTestWindow>();
            xg.imui.Panel.Open<ManagedFloatingWindow>();
            xg.imui.Panel.Open<ManagedSceneViewWindow>();
            if (Environment.GetEnvironmentVariable("XG_UI_TEST_FOCUS") == "managed")
                _testWindow.Focus();
            Debug.Log($"[Debug] Managed Test registered: {_testWindow.IsRegistered}, xgImUIKit API {xg.imui.ImUI.NativeApiVersion}");
            return 1;
        }

        static ManagedTestWindow _testWindow = null!;

        static PlayerComponent _player = null!;
        static xg.Actor _temporary = null!;
        static EnemyComponent _temporaryEnemy = null!;
        static bool _reported;

        static readonly System.Diagnostics.Stopwatch _clock = new();
        static float _elapsedAtStart;

        [UnmanagedCallersOnly]
        public static void Update(float dt)
        {
            long frames = ActorRegistry.FrameCount;
            if (frames == 1 && !_clock.IsRunning)
            {
                _elapsedAtStart = _player.ElapsedTime;
                _clock.Start();
            }
            if (_temporary.IsValid && frames >= 30)
            {
                _temporary.Destroy();
                Debug.Log($"[Actors] destroyed 'temporary' after {frames} actor frames (EnemyComponent updated {_temporaryEnemy.UpdateCount} times)");
            }
            if (!_reported && frames >= 61)
            {
                _reported = true;
                double wall = _clock.Elapsed.TotalSeconds;
                double summed = _player.ElapsedTime - _elapsedAtStart;
                Debug.Log($"[Actors] CoreCLR: {ActorRegistry.Count} actor(s) after {frames} actor frames: " +
                    $"PlayerComponent updated {_player.UpdateCount} times, destroyed actor's EnemyComponent {_temporaryEnemy.UpdateCount} times");
                Debug.Log($"[Actors] delta time over 60 frames: summed dt {summed:F4} s, wall clock {wall:F4} s, " +
                    $"difference {(summed - wall) * 1000.0:F2} ms, average frame {wall / 60.0 * 1000.0:F2} ms");
            }
        }

        [UnmanagedCallersOnly]
        public static void Shutdown()
        {
            xg.imui.ImUI.ShutdownAll();
            EditorLog.Detach();
            ScriptRuntime.Shutdown();
        }

        private static void OnLog(MessageType type, IntPtr msgPtr)
        {
            string msg = Marshal.PtrToStringAnsi(msgPtr) ?? string.Empty;
           // Console.WriteLine($"[{type}] {msg} waasdadsds");
        }

        public class TestType
        {
            public int Health { get; set; }
            public string Name { get; set; } = string.Empty;
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
                    if (obj2 is not null)
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

        public static void TestQuaternion()
        {
            Console.WriteLine("=== XGE Quaternion Interop Test ===");

            Quaternion qA = new Quaternion(0, 0.7071067f, 0, 0.7071067f); // 90° around Y
            Quaternion qB = new Quaternion(0.7071067f, 0, 0, 0.7071067f); // 90° around X

            // Arithmetic
            Quaternion add = qA.Add(qB);
            Console.WriteLine($"Add: {add.x}, {add.y}, {add.z}, {add.w}");

            Quaternion sub = qA.Sub(qB);
            Console.WriteLine($"Sub: {sub.x}, {sub.y}, {sub.z}, {sub.w}");

            Quaternion mulScalar = qA.Mul(2.0f);
            Console.WriteLine($"MulScalar: {mulScalar.x}, {mulScalar.y}, {mulScalar.z}, {mulScalar.w}");

            // Quaternion math
            Console.WriteLine($"Length(qA): {qA.Length()}");
            Console.WriteLine($"LengthSquared(qA): {qA.LengthSquared()}");

            Quaternion norm = qA.Normalized();
            Console.WriteLine($"Normalize(qA): {norm.x}, {norm.y}, {norm.z}, {norm.w}");

            Console.WriteLine($"Dot(qA,qB): {qA.Dot(qB)}");

            Quaternion conj = qA.Conjugate();
            Console.WriteLine($"Conjugate(qA): {conj.x}, {conj.y}, {conj.z}, {conj.w}");

            Quaternion inv = qA.Inverse();
            Console.WriteLine($"Inverse(qA): {inv.x}, {inv.y}, {inv.z}, {inv.w}");

            Quaternion mul = qA.Mul(qB);
            Console.WriteLine($"MulQuat(qA,qB): {mul.x}, {mul.y}, {mul.z}, {mul.w}");

            // Slerp
            Quaternion slerp = qA.Slerp(qB, 0.5f);
            Console.WriteLine($"Slerp(qA,qB,0.5): {slerp.x}, {slerp.y}, {slerp.z}, {slerp.w}");

            // Rotate vector
            Vector3 v = new Vector3(1, 0, 0);
            Vector3 vRot = qA.RotateVec3(v);
            Console.WriteLine($"RotateVec3(qA, (1,0,0)): {vRot.x}, {vRot.y}, {vRot.z}");

            // Identity check
            Console.WriteLine($"IsIdentity(qA): {qA.IsIdentity()}");

            // Equality
            Console.WriteLine($"IsEqual(qA,qB): {qA.IsEqual(qB)}");
            Console.WriteLine($"IsEqual(qA,qA): {qA.IsEqual(qA)}");

            Console.WriteLine("=== XGE Quaternion Interop Test Complete ===");
        }

        public static void TestMatrix4x4()
        {
            Console.WriteLine("=== XGE Matrix4x4 Interop Test ===");

            Matrix4x4 I = Matrix4x4.Identity();
            Console.WriteLine("Identity:");
            Console.WriteLine(I);

            Matrix4x4 T = Matrix4x4.Translation(3, 4, 5);
            Console.WriteLine("Translation(3,4,5):");
            Console.WriteLine(T);

            Matrix4x4 S = Matrix4x4.Scaling(2, 2, 2);
            Console.WriteLine("Scaling(2,2,2):");
            Console.WriteLine(S);

            Matrix4x4 RY = Matrix4x4.RotationY(MathF.PI * 0.5f);
            Console.WriteLine("RotationY(90°):");
            Console.WriteLine(RY);

            Matrix4x4 M = T * RY * S;
            Console.WriteLine("M = T * RY * S:");
            Console.WriteLine(M);

            Matrix4x4 MT = M.Transposed();
            Console.WriteLine("Transpose(M):");
            Console.WriteLine(MT);

            Matrix4x4 MI = M.Inverse();
            Console.WriteLine("Inverse(M):");
            Console.WriteLine(MI);

            float det = M.Determinant();
            Console.WriteLine($"Determinant(M): {det}");

            Vector3 p = new Vector3(1, 2, 3);
            Vector3 p2 = M.TransformPoint(p);
            Console.WriteLine($"TransformPoint(M, (1,2,3)): {p2}");

            Vector3 d = new Vector3(0, 1, 0);
            Vector3 d2 = M.TransformDirection(d);
            Console.WriteLine($"TransformDirection(M, (0,1,0)): {d2}");

            M.Decompose(out Vector3 scale, out Quaternion rot, out Vector3 trans);
            Console.WriteLine("Decompose(M):");
            Console.WriteLine($"  Scale: {scale}");
            Console.WriteLine($"  Rotation: {rot}");
            Console.WriteLine($"  Translation: {trans}");

            Console.WriteLine("=== XGE Matrix4x4 Interop Test Complete ===");
        }

    }
}
