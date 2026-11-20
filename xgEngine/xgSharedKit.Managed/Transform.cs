// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg.math;

namespace xg
{
    // Mirror of the native xg::TransformData (40 bytes): position, rotation quaternion (x, y, z, w), scale.
    [StructLayout(LayoutKind.Sequential)]
    public struct TransformData
    {
        public float PositionX, PositionY, PositionZ;
        public float RotationX, RotationY, RotationZ, RotationW;
        public float ScaleX, ScaleY, ScaleZ;

        public static TransformData Identity => new() { RotationW = 1, ScaleX = 1, ScaleY = 1, ScaleZ = 1 };

        public Vector3 Position
        {
            readonly get => new(PositionX, PositionY, PositionZ);
            set { PositionX = value.x; PositionY = value.y; PositionZ = value.z; }
        }

        public Quaternion Rotation
        {
            readonly get => new(RotationX, RotationY, RotationZ, RotationW);
            set { RotationX = value.x; RotationY = value.y; RotationZ = value.z; RotationW = value.w; }
        }

        public Vector3 Scale
        {
            readonly get => new(ScaleX, ScaleY, ScaleZ);
            set { ScaleX = value.x; ScaleY = value.y; ScaleZ = value.z; }
        }
    }

    // An actor's native transform (xg::TransformSystem). The proxy holds only the actor id: every access reads or writes
    // the native store, so all modules see the same values. Accessing an actor without a transform throws.
    [ComponentIcon("Transform")]
    public sealed unsafe partial class Transform : IComponent, IEquatable<Transform>
    {
        public ActorID ActorID { get; }

        internal Transform(ActorID id) => ActorID = id;

        // A proxy for any actor (including other modules' actors); check IsValid before use.
        public static Transform Of(ActorID id) => new(id);

        public static bool Exists(ActorID id) => ScriptRuntime._enginePtr != 0 && XG_TransformHas(ScriptRuntime._enginePtr, id) != 0;

        public bool IsValid => Exists(ActorID);

        public TransformData Local
        {
            get { TransformData t; Check(XG_TransformGetLocal(ScriptRuntime._enginePtr, ActorID, &t)); return t; }
            set => Check(XG_TransformSetLocal(ScriptRuntime._enginePtr, ActorID, &value));
        }

        public TransformData World
        {
            get { TransformData t; Check(XG_TransformGetWorld(ScriptRuntime._enginePtr, ActorID, &t)); return t; }
            set => Check(XG_TransformSetWorld(ScriptRuntime._enginePtr, ActorID, &value));
        }

        public Vector3 LocalPosition
        {
            get => Local.Position;
            set { TransformData t = Local; t.Position = value; Local = t; }
        }

        public Quaternion LocalRotation
        {
            get => Local.Rotation;
            set { TransformData t = Local; t.Rotation = value; Local = t; }
        }

        public Vector3 LocalScale
        {
            get => Local.Scale;
            set { TransformData t = Local; t.Scale = value; Local = t; }
        }

        public Vector3 Position
        {
            get => World.Position;
            set { TransformData t = World; t.Position = value; World = t; }
        }

        public Quaternion Rotation
        {
            get => World.Rotation;
            set { TransformData t = World; t.Rotation = value; World = t; }
        }

        // Euler angles in degrees (x = pitch, y = yaw, z = roll; roll, then pitch, then yaw).
        public Vector3 LocalEulerAngles
        {
            get => ToEuler(LocalRotation);
            set => LocalRotation = FromEuler(value);
        }

        // Row-major for row vectors (v * M).
        public Matrix4x4 LocalToWorld
        {
            get { Matrix4x4 m; Check(XG_TransformGetWorldMatrix(ScriptRuntime._enginePtr, ActorID, (float*)&m)); return m; }
        }

        public void GetLocal(out Vector3 position, out Quaternion rotation, out Vector3 scale)
        {
            TransformData t = Local;
            position = t.Position;
            rotation = t.Rotation;
            scale = t.Scale;
        }

        public void SetLocal(Vector3 position, Quaternion rotation, Vector3 scale)
        {
            Local = new TransformData { Position = position, Rotation = rotation, Scale = scale };
        }

        // The nearest ancestor that has a transform, or null.
        public Transform? Parent
        {
            get
            {
                for (ActorID? id = ActorTree.GetParent(ActorID); id is ActorID parent; id = ActorTree.GetParent(parent))
                {
                    if (Exists(parent))
                        return new Transform(parent);
                }
                return null;
            }
        }

        // Reparents the actor in the hierarchy; keepWorld keeps its world pose.
        public bool SetParent(Transform? parent, bool keepWorld = true) => ActorTree.SetParent(ActorID, parent?.ActorID, keepWorld);

        public static int GetLocals(ReadOnlySpan<ActorID> actors, Span<TransformData> output)
        {
            if (output.Length < actors.Length)
                throw new ArgumentException("output is shorter than actors", nameof(output));
            fixed (ActorID* ids = actors)
            fixed (TransformData* o = output)
                return XG_TransformGetLocals(ScriptRuntime._enginePtr, (ulong*)ids, actors.Length, o);
        }

        public static int SetLocals(ReadOnlySpan<ActorID> actors, ReadOnlySpan<TransformData> values)
        {
            if (values.Length < actors.Length)
                throw new ArgumentException("values is shorter than actors", nameof(values));
            fixed (ActorID* ids = actors)
            fixed (TransformData* v = values)
                return XG_TransformSetLocals(ScriptRuntime._enginePtr, (ulong*)ids, actors.Length, v);
        }

        public static int GetWorlds(ReadOnlySpan<ActorID> actors, Span<TransformData> output)
        {
            if (output.Length < actors.Length)
                throw new ArgumentException("output is shorter than actors", nameof(output));
            fixed (ActorID* ids = actors)
            fixed (TransformData* o = output)
                return XG_TransformGetWorlds(ScriptRuntime._enginePtr, (ulong*)ids, actors.Length, o);
        }

        public static int SetWorlds(ReadOnlySpan<ActorID> actors, ReadOnlySpan<TransformData> values)
        {
            if (values.Length < actors.Length)
                throw new ArgumentException("values is shorter than actors", nameof(values));
            fixed (ActorID* ids = actors)
            fixed (TransformData* v = values)
                return XG_TransformSetWorlds(ScriptRuntime._enginePtr, (ulong*)ids, actors.Length, v);
        }

        public static ulong CurrentVersion => ScriptRuntime._enginePtr == 0 ? 0 : XG_TransformGetVersion(ScriptRuntime._enginePtr);

        // Actors whose transform changed after since (see CurrentVersion).
        public static void CollectChanged(ulong since, List<ActorID> result)
        {
            result.Clear();
            nint engine = ScriptRuntime._enginePtr;
            if (engine == 0)
                return;
            int count = XG_TransformCollectChanged(engine, since, null, 0);
            if (count == 0)
                return;
            ulong[] ids = new ulong[count];
            fixed (ulong* p = ids)
                count = Math.Min(XG_TransformCollectChanged(engine, since, p, count), count);
            for (int i = 0; i < count; i++)
                result.Add(new ActorID(ids[i]));
        }

        // Every actor that has a transform (any module).
        public static void GetActors(List<ActorID> result)
        {
            result.Clear();
            nint engine = ScriptRuntime._enginePtr;
            if (engine == 0)
                return;
            int count = XG_TransformGetActors(engine, null, 0);
            if (count == 0)
                return;
            ulong[] ids = new ulong[count];
            fixed (ulong* p = ids)
                count = Math.Min(XG_TransformGetActors(engine, p, count), count);
            for (int i = 0; i < count; i++)
                result.Add(new ActorID(ids[i]));
        }

        // Quaternion from Euler degrees (x = pitch, y = yaw, z = roll), the convention of XMQuaternionRotationRollPitchYaw.
        public static Quaternion FromEuler(Vector3 degrees)
        {
            const float half = MathF.PI / 360.0f;
            float sp = MathF.Sin(degrees.x * half), cp = MathF.Cos(degrees.x * half);
            float sy = MathF.Sin(degrees.y * half), cy = MathF.Cos(degrees.y * half);
            float sr = MathF.Sin(degrees.z * half), cr = MathF.Cos(degrees.z * half);
            return new Quaternion(
                sp * cy * cr + cp * sy * sr,
                cp * sy * cr - sp * cy * sr,
                cp * cy * sr - sp * sy * cr,
                cp * cy * cr + sp * sy * sr);
        }

        public static Vector3 ToEuler(Quaternion q)
        {
            const float toDegrees = 180.0f / MathF.PI;
            float sinPitch = Math.Clamp(2.0f * (q.w * q.x - q.y * q.z), -1.0f, 1.0f);
            float pitch = MathF.Asin(sinPitch);
            float yaw = MathF.Atan2(2.0f * (q.x * q.z + q.w * q.y), 1.0f - 2.0f * (q.x * q.x + q.y * q.y));
            float roll = MathF.Atan2(2.0f * (q.x * q.y + q.w * q.z), 1.0f - 2.0f * (q.x * q.x + q.z * q.z));
            static float Clean(float v) => MathF.Abs(v) < 1e-4f ? 0.0f : v;
            return new Vector3(Clean(pitch * toDegrees), Clean(yaw * toDegrees), Clean(roll * toDegrees));
        }

        // Local poses of every actor that has a transform; Restore puts them back (play mode).
        public sealed class Snapshot
        {
            private readonly ActorID[] _actors;
            private readonly TransformData[] _locals;

            private Snapshot(ActorID[] actors, TransformData[] locals)
            {
                _actors = actors;
                _locals = locals;
            }

            public int Count => _actors.Length;

            public static Snapshot Capture()
            {
                List<ActorID> actors = new();
                GetActors(actors);
                ActorID[] ids = actors.ToArray();
                TransformData[] locals = new TransformData[ids.Length];
                GetLocals(ids, locals);
                return new Snapshot(ids, locals);
            }

            // Re-adds transforms removed since the capture, restores the local poses and removes transforms added since.
            // Actors destroyed meanwhile are skipped.
            public (int Restored, int Readded, int Removed) Restore()
            {
                HashSet<ulong> captured = new();
                int readded = 0;
                foreach (ActorID id in _actors)
                {
                    captured.Add(id);
                    if (!Exists(id) && ActorTree.GetName(id).Length > 0 && Add(id))
                        readded++;
                }
                int restored = SetLocals(_actors, _locals);
                List<ActorID> current = new();
                GetActors(current);
                int removed = 0;
                foreach (ActorID id in current)
                    if (!captured.Contains(id) && Remove(id))
                        removed++;
                return (restored, readded, removed);
            }
        }

        internal static bool Add(ActorID id) => ScriptRuntime._enginePtr != 0 && XG_TransformAdd(ScriptRuntime._enginePtr, id) != 0;
        internal static bool Remove(ActorID id) => ScriptRuntime._enginePtr != 0 && XG_TransformRemove(ScriptRuntime._enginePtr, id) != 0;

        public bool Equals(Transform? other) => other is not null && (ulong)other.ActorID == (ulong)ActorID;
        public override bool Equals(object? obj) => obj is Transform other && Equals(other);
        public override int GetHashCode() => ActorID.GetHashCode();
        public override string ToString() => $"Transform({(ulong)ActorID})";

        private void Check(byte ok)
        {
            if (ok == 0)
                throw new InvalidOperationException($"Actor {(ulong)ActorID} has no Transform.");
        }

        [LibraryImport("xgGameKernel", EntryPoint = "XG_TransformAdd")]
        private static partial byte XG_TransformAdd(nint engine, ulong id);

        [LibraryImport("xgGameKernel", EntryPoint = "XG_TransformRemove")]
        private static partial byte XG_TransformRemove(nint engine, ulong id);

        [LibraryImport("xgGameKernel", EntryPoint = "XG_TransformHas")]
        private static partial byte XG_TransformHas(nint engine, ulong id);

        [LibraryImport("xgGameKernel", EntryPoint = "XG_TransformGetLocal")]
        private static partial byte XG_TransformGetLocal(nint engine, ulong id, TransformData* output);

        [LibraryImport("xgGameKernel", EntryPoint = "XG_TransformSetLocal")]
        private static partial byte XG_TransformSetLocal(nint engine, ulong id, TransformData* value);

        [LibraryImport("xgGameKernel", EntryPoint = "XG_TransformGetWorld")]
        private static partial byte XG_TransformGetWorld(nint engine, ulong id, TransformData* output);

        [LibraryImport("xgGameKernel", EntryPoint = "XG_TransformSetWorld")]
        private static partial byte XG_TransformSetWorld(nint engine, ulong id, TransformData* value);

        [LibraryImport("xgGameKernel", EntryPoint = "XG_TransformGetWorldMatrix")]
        private static partial byte XG_TransformGetWorldMatrix(nint engine, ulong id, float* output);

        [LibraryImport("xgGameKernel", EntryPoint = "XG_TransformGetLocals")]
        private static partial int XG_TransformGetLocals(nint engine, ulong* ids, int count, TransformData* output);

        [LibraryImport("xgGameKernel", EntryPoint = "XG_TransformSetLocals")]
        private static partial int XG_TransformSetLocals(nint engine, ulong* ids, int count, TransformData* values);

        [LibraryImport("xgGameKernel", EntryPoint = "XG_TransformGetWorlds")]
        private static partial int XG_TransformGetWorlds(nint engine, ulong* ids, int count, TransformData* output);

        [LibraryImport("xgGameKernel", EntryPoint = "XG_TransformSetWorlds")]
        private static partial int XG_TransformSetWorlds(nint engine, ulong* ids, int count, TransformData* values);

        [LibraryImport("xgGameKernel", EntryPoint = "XG_TransformGetVersion")]
        private static partial ulong XG_TransformGetVersion(nint engine);

        [LibraryImport("xgGameKernel", EntryPoint = "XG_TransformGetActors")]
        private static partial int XG_TransformGetActors(nint engine, ulong* output, int capacity);

        [LibraryImport("xgGameKernel", EntryPoint = "XG_TransformCollectChanged")]
        private static partial int XG_TransformCollectChanged(nint engine, ulong since, ulong* output, int capacity);
    }
}
