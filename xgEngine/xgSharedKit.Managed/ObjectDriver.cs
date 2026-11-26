// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

namespace xg
{
    [Flags]
    public enum ObjectDriverFlags : uint
    {
        None = 0,
        Hidden = 1u << 0,
    }

    // Mirror of the native xg::ObjectDriverData (24 bytes) in the engine's "xg.ObjectDriver" pool.
    [StructLayout(LayoutKind.Sequential)]
    [ComponentDataKey("xg.ObjectDriver")]
    public struct ObjectDriverData : IComponentData
    {
        [AssetType("object")]
        public AssetReference Object;
        public ObjectDriverFlags Flags;
        [HideInInspector]
        public uint Reserved;
    }

    // Holds and draws a composition object (.xco) at its actor, in every render view (edit mode too). The engine's object
    // drive system reads the data, so an ObjectDriverComponent works the same from any module.
    [ComponentIcon("Object", Color = "#C9A24A")]
    [RequireComponent(typeof(Transform))]
    public sealed partial class ObjectDriverComponent : ComponentDataSet<ObjectDriverData>
    {
        public AssetReference Object
        {
            get => Data1.Object;
            set => Data1.Object = value;
        }

        public bool Hidden
        {
            get => (Data1.Flags & ObjectDriverFlags.Hidden) != 0;
            set => Data1.Flags = value ? Data1.Flags | ObjectDriverFlags.Hidden : Data1.Flags & ~ObjectDriverFlags.Hidden;
        }

        // World matrix (16 floats, row-major for row vectors) of a named Object of the composition: an exposed
        // Composition Proxy first, else any Object with that name. False until the object is loaded.
        public unsafe bool TryGetNodeMatrix(string name, Span<float> world)
        {
            if (world.Length < 16 || ScriptRuntime._enginePtr == 0 || string.IsNullOrEmpty(name))
                return false;
            fixed (float* destination = world)
                return Native.GetNodeWorld(ScriptRuntime._enginePtr, ActorID.Value, name, destination) != 0;
        }

        // World pose of a Composition Proxy (or any Object) by name, e.g. the barrel point of a gun.
        public bool TryGetProxy(string name, out TransformData world)
        {
            world = TransformData.Identity;
            Span<float> m = stackalloc float[16];
            if (!TryGetNodeMatrix(name, m))
                return false;
            world = Decompose(m);
            return true;
        }

        // Splits a scale * rotation * translation matrix (row vectors) into a pose.
        public static TransformData Decompose(ReadOnlySpan<float> m)
        {
            TransformData pose = TransformData.Identity;
            pose.PositionX = m[12];
            pose.PositionY = m[13];
            pose.PositionZ = m[14];
            float sx = MathF.Sqrt(m[0] * m[0] + m[1] * m[1] + m[2] * m[2]);
            float sy = MathF.Sqrt(m[4] * m[4] + m[5] * m[5] + m[6] * m[6]);
            float sz = MathF.Sqrt(m[8] * m[8] + m[9] * m[9] + m[10] * m[10]);
            pose.ScaleX = sx;
            pose.ScaleY = sy;
            pose.ScaleZ = sz;
            if (sx <= 0.0f || sy <= 0.0f || sz <= 0.0f)
                return pose;
            float r00 = m[0] / sx, r01 = m[1] / sx, r02 = m[2] / sx;
            float r10 = m[4] / sy, r11 = m[5] / sy, r12 = m[6] / sy;
            float r20 = m[8] / sz, r21 = m[9] / sz, r22 = m[10] / sz;
            float trace = r00 + r11 + r22;
            float x, y, z, w;
            if (trace > 0.0f)
            {
                float s = MathF.Sqrt(trace + 1.0f) * 2.0f;
                w = 0.25f * s;
                x = (r12 - r21) / s;
                y = (r20 - r02) / s;
                z = (r01 - r10) / s;
            }
            else if (r00 > r11 && r00 > r22)
            {
                float s = MathF.Sqrt(1.0f + r00 - r11 - r22) * 2.0f;
                w = (r12 - r21) / s;
                x = 0.25f * s;
                y = (r01 + r10) / s;
                z = (r20 + r02) / s;
            }
            else if (r11 > r22)
            {
                float s = MathF.Sqrt(1.0f + r11 - r00 - r22) * 2.0f;
                w = (r20 - r02) / s;
                x = (r01 + r10) / s;
                y = 0.25f * s;
                z = (r12 + r21) / s;
            }
            else
            {
                float s = MathF.Sqrt(1.0f + r22 - r00 - r11) * 2.0f;
                w = (r01 - r10) / s;
                x = (r20 + r02) / s;
                y = (r12 + r21) / s;
                z = 0.25f * s;
            }
            pose.RotationX = x;
            pose.RotationY = y;
            pose.RotationZ = z;
            pose.RotationW = w;
            return pose;
        }

        // The Composition Actor spawned for an Object exposed as an actor (ActorID.InvalidID until the object loads).
        public ActorID GetCompositionActor(string name) =>
            ScriptRuntime._enginePtr == 0 || string.IsNullOrEmpty(name) ? ActorID.InvalidID : new ActorID(Native.GetActor(ScriptRuntime._enginePtr, ActorID.Value, name));

        // The Composition Actor of any driver actor by Object name (works for drivers of every module).
        public static ActorID GetCompositionActor(ActorID driver, string name) =>
            ScriptRuntime._enginePtr == 0 || string.IsNullOrEmpty(name) ? ActorID.InvalidID : new ActorID(Native.GetActor(ScriptRuntime._enginePtr, driver.Value, name));

        // The driver actor that owns a Composition Actor (ActorID.InvalidID when the actor isn't one). Composition Actors
        // are added and removed only by their composition, and their transforms follow its Jointers.
        public static ActorID CompositionOwner(ActorID actor) =>
            ScriptRuntime._enginePtr == 0 ? ActorID.InvalidID : new ActorID(Native.GetOwner(ScriptRuntime._enginePtr, actor.Value));

        public static bool IsCompositionActor(ActorID actor) => CompositionOwner(actor).Value != 0;

        // Takes the next spawned Composition Actor waiting for its authored components (a JSON array of
        // { "type": "<Full.Name, Assembly>", "fields": { ... } }). False when none is waiting.
        public static unsafe bool TryTakeSpawned(out ActorID actor, out string json)
        {
            actor = ActorID.InvalidID;
            json = string.Empty;
            if (ScriptRuntime._enginePtr == 0)
                return false;
            ulong id;
            int length = Native.TakeSpawned(ScriptRuntime._enginePtr, &id, null, 0);
            if (length < 0)
                return false;
            byte[] buffer = new byte[length + 1];
            fixed (byte* p = buffer)
                length = Native.TakeSpawned(ScriptRuntime._enginePtr, &id, p, buffer.Length);
            if (length < 0)
                return false;
            actor = new ActorID(id);
            json = System.Text.Encoding.UTF8.GetString(buffer, 0, length);
            return true;
        }

        // Draws added last frame by the object drive system, and how many object instances were loaded.
        public static int DrawCount(out int readyObjects)
        {
            readyObjects = 0;
            return ScriptRuntime._enginePtr == 0 ? 0 : Native.Stats(ScriptRuntime._enginePtr, out readyObjects);
        }

        private static unsafe partial class Native
        {
            [LibraryImport("xgScriptNative", EntryPoint = "XG_ObjectDriveStats")]
            public static partial int Stats(nint engine, out int readyObjects);

            [LibraryImport("xgScriptNative", EntryPoint = "XG_ObjectDriverGetNodeWorld", StringMarshalling = StringMarshalling.Utf8)]
            public static partial byte GetNodeWorld(nint engine, ulong actor, string name, float* world);

            [LibraryImport("xgScriptNative", EntryPoint = "XG_ObjectDriverGetActor", StringMarshalling = StringMarshalling.Utf8)]
            public static partial ulong GetActor(nint engine, ulong driver, string name);

            [LibraryImport("xgScriptNative", EntryPoint = "XG_ObjectDriverGetOwner")]
            public static partial ulong GetOwner(nint engine, ulong actor);

            [LibraryImport("xgScriptNative", EntryPoint = "XG_ObjectDriverTakeSpawned")]
            public static partial int TakeSpawned(nint engine, ulong* actor, byte* json, int capacity);
        }
    }
}
