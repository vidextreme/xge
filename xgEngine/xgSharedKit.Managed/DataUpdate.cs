// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

namespace xg
{
    // What an OnDataUpdate call gets besides its data: the frame's delta time, the element's actor and the job's command
    // buffer for anything that isn't the element itself (other actors, other pools, structural changes).
    public readonly struct DataUpdateContext
    {
        internal DataUpdateContext(float deltaTime, ActorID actor, CommandBuffer commands)
        {
            DeltaTime = deltaTime;
            Actor = actor;
            Commands = commands;
        }

        public float DeltaTime { get; }
        public ActorID Actor { get; }
        public CommandBuffer Commands { get; }
    }

    // Implemented through the IComponentDataUpdate interfaces; ComponentDataSet registers the update when it attaches.
    public interface IDataUpdateRegistrar<TData> where TData : unmanaged, IComponentData
    {
        void RegisterDataUpdate();
    }

    // ECS-style update of every element of TWrite (the whole pool, whichever component added it), possibly on several job
    // workers at once. The update may change only its own element; reads come in as `in` parameters (elements of the same
    // actor; elements whose actor lacks a read type are skipped). One module updates a data type: the first to claim it.
    public interface IComponentDataUpdate<TSelf, TWrite> : IDataUpdateRegistrar<TWrite>
        where TSelf : IComponentDataUpdate<TSelf, TWrite>
        where TWrite : unmanaged, IComponentData
    {
        static abstract void OnDataUpdate(ref TWrite data, in DataUpdateContext context);
        void IDataUpdateRegistrar<TWrite>.RegisterDataUpdate() => DataUpdateGroup<TSelf, TWrite>.Instance.Register();
    }

    public interface IComponentDataUpdate<TSelf, TWrite, TRead1> : IDataUpdateRegistrar<TWrite>
        where TSelf : IComponentDataUpdate<TSelf, TWrite, TRead1>
        where TWrite : unmanaged, IComponentData
        where TRead1 : unmanaged, IComponentData
    {
        static abstract void OnDataUpdate(ref TWrite data, in TRead1 read1, in DataUpdateContext context);
        void IDataUpdateRegistrar<TWrite>.RegisterDataUpdate() => DataUpdateGroup<TSelf, TWrite, TRead1>.Instance.Register();
    }

    public interface IComponentDataUpdate<TSelf, TWrite, TRead1, TRead2> : IDataUpdateRegistrar<TWrite>
        where TSelf : IComponentDataUpdate<TSelf, TWrite, TRead1, TRead2>
        where TWrite : unmanaged, IComponentData
        where TRead1 : unmanaged, IComponentData
        where TRead2 : unmanaged, IComponentData
    {
        static abstract void OnDataUpdate(ref TWrite data, in TRead1 read1, in TRead2 read2, in DataUpdateContext context);
        void IDataUpdateRegistrar<TWrite>.RegisterDataUpdate() => DataUpdateGroup<TSelf, TWrite, TRead1, TRead2>.Instance.Register();
    }

    public interface IComponentDataUpdate<TSelf, TWrite, TRead1, TRead2, TRead3> : IDataUpdateRegistrar<TWrite>
        where TSelf : IComponentDataUpdate<TSelf, TWrite, TRead1, TRead2, TRead3>
        where TWrite : unmanaged, IComponentData
        where TRead1 : unmanaged, IComponentData
        where TRead2 : unmanaged, IComponentData
        where TRead3 : unmanaged, IComponentData
    {
        static abstract void OnDataUpdate(ref TWrite data, in TRead1 read1, in TRead2 read2, in TRead3 read3, in DataUpdateContext context);
        void IDataUpdateRegistrar<TWrite>.RegisterDataUpdate() => DataUpdateGroup<TSelf, TWrite, TRead1, TRead2, TRead3>.Instance.Register();
    }

    // Deferred work recorded during a data update and played back on the main thread after the pass, in job order.
    public sealed unsafe class CommandBuffer
    {
        private enum Op : byte
        {
            SetData,
            AddData,
            RemoveData,
            SetLocalTransform,
            SetWorldTransform,
            Destroy,
            Invoke,
        }

        private struct Entry
        {
            public Op Op;
            public uint Type;
            public ulong Actor;
            public int Offset;
            public int Size;
            public Action? Action;
            public string? Name;
        }

        private readonly List<Entry> _entries = new();
        private byte[] _bytes = new byte[256];
        private int _used;

        public int Count => _entries.Count;

        // Writes the actor's element of T, if it has one.
        public void SetData<T>(ActorID actor, in T value) where T : unmanaged, IComponentData => Record(Op.SetData, ComponentDataType<T>.Id, actor, value, typeof(T).Name);

        // Adds a reference to the actor's element of T (creating it) and writes it.
        public void AddData<T>(ActorID actor, in T value) where T : unmanaged, IComponentData => Record(Op.AddData, ComponentDataType<T>.Id, actor, value, typeof(T).Name);

        public void RemoveData<T>(ActorID actor) where T : unmanaged, IComponentData =>
            _entries.Add(new Entry { Op = Op.RemoveData, Type = ComponentDataType<T>.Id, Actor = actor, Name = typeof(T).Name });

        public void SetLocalTransform(ActorID actor, in TransformData value) => Record(Op.SetLocalTransform, 0, actor, value, null);
        public void SetWorldTransform(ActorID actor, in TransformData value) => Record(Op.SetWorldTransform, 0, actor, value, null);

        public void Destroy(ActorID actor) => _entries.Add(new Entry { Op = Op.Destroy, Actor = actor });

        // Runs on the main thread at playback.
        public void Invoke(Action action) => _entries.Add(new Entry { Op = Op.Invoke, Action = action });

        private void Record<T>(Op op, uint type, ActorID actor, in T value, string? name) where T : unmanaged
        {
            int size = sizeof(T);
            if (_used + size > _bytes.Length)
                Array.Resize(ref _bytes, Math.Max(_bytes.Length * 2, _used + size));
            fixed (byte* p = &_bytes[_used])
                *(T*)p = value;
            _entries.Add(new Entry { Op = op, Type = type, Actor = actor, Offset = _used, Size = size, Name = name });
            _used += size;
        }

        internal void Playback()
        {
            nint engine = ScriptRuntime._enginePtr;
            foreach (Entry e in _entries)
            {
                try
                {
                    Apply(engine, e);
                }
                catch (Exception ex)
                {
                    Debug.LogException(ex);
                }
            }
            Clear();
        }

        private void Apply(nint engine, Entry e)
        {
            switch (e.Op)
            {
                case Op.SetData:
                case Op.AddData:
                {
                    if (e.Type == 0)
                        return;
                    if (e.Op == Op.AddData)
                    {
                        ComponentData.Native.GetTypeInfo(engine, e.Type, out _, out _, out _, out uint flags);
                        if ((flags & (uint)ComponentDataFlags.EngineOwned) != 0)
                        {
                            Debug.LogError($"[ComponentData] CommandBuffer.AddData: {e.Name} is owned by the engine.");
                            return;
                        }
                    }
                    void* element = e.Op == Op.AddData ? ComponentData.Native.Add(engine, e.Type, e.Actor) : ComponentData.Native.Get(engine, e.Type, e.Actor);
                    if (element != null)
                        _bytes.AsSpan(e.Offset, e.Size).CopyTo(new Span<byte>(element, e.Size));
                    return;
                }
                case Op.RemoveData:
                    if (e.Type != 0)
                        ComponentData.Native.Remove(engine, e.Type, e.Actor);
                    return;
                case Op.SetLocalTransform:
                case Op.SetWorldTransform:
                {
                    TransformData value;
                    fixed (byte* p = &_bytes[e.Offset])
                        value = *(TransformData*)p;
                    Transform t = Transform.Of(e.Actor);
                    if (!t.IsValid)
                        return;
                    if (e.Op == Op.SetLocalTransform)
                        t.Local = value;
                    else
                        t.World = value;
                    return;
                }
                case Op.Destroy:
                    if (ActorRegistry.TryGet(e.Actor, out Actor? actor))
                        actor.Destroy();
                    else
                        Actor.ActorUtils.XG_DestroyActor(engine, e.Actor);
                    return;
                case Op.Invoke:
                    e.Action?.Invoke();
                    return;
            }
        }

        internal void Clear()
        {
            _entries.Clear();
            _used = 0;
        }
    }

    // One registered data update: runs over the write pool, inline or split into job ranges, then plays back the command
    // buffers in range order and stamps the pool as changed.
    internal abstract unsafe class DataUpdateGroup
    {
        internal const int ParallelThreshold = 2048;
        internal const int RangeSize = 512;

        private static readonly List<DataUpdateGroup> _groups = new();

        private bool _registered;
        private bool _claimed;
        private bool _blocked;
        private bool _failed;
        private uint _writeType;
        private readonly List<Range> _ranges = new();
        private readonly List<JobHandle> _jobs = new();
        private readonly uint[] _readTypes;
        protected readonly int[][] ReadSlots;
        protected readonly nint[] ReadDense;

        protected DataUpdateGroup(int readCount)
        {
            _readTypes = new uint[readCount];
            ReadSlots = new int[readCount][];
            ReadDense = new nint[readCount];
            for (int i = 0; i < readCount; i++)
                ReadSlots[i] = Array.Empty<int>();
        }

        protected abstract string Name { get; }
        protected abstract uint WriteTypeId { get; }
        protected abstract bool WriteEngineOwned { get; }
        protected abstract void ReadTypeIds(uint[] ids);
        protected abstract void RunRange(void* write, ulong* owners, int start, int end, float dt, CommandBuffer commands);

        public void Register()
        {
            if (_registered)
                return;
            _registered = true;
            _groups.Add(this);
        }

        internal static void UpdateAll(float dt)
        {
            for (int i = 0; i < _groups.Count; i++)
                _groups[i].Update(dt);
        }

        internal static void ClearAll(nint engine)
        {
            foreach (DataUpdateGroup group in _groups)
            {
                if (group._claimed && engine != 0)
                    ComponentData.Native.ReleaseUpdater(engine, group._writeType, ScriptRuntime.ModuleID);
                group._registered = false;
                group._claimed = false;
                group._blocked = false;
                group._failed = false;
            }
            _groups.Clear();
        }

        private void Update(float dt)
        {
            nint engine = ScriptRuntime._enginePtr;
            if (_blocked || engine == 0)
                return;
            if (!_claimed)
            {
                uint write = WriteTypeId;
                if (write == 0 || WriteEngineOwned)
                {
                    _blocked = true;
                    Debug.LogError($"[ComponentData] {Name}: the written type is {(write == 0 ? "not registered" : "owned by the engine")}.");
                    return;
                }
                if (!ComponentData.Native.ClaimUpdater(engine, write, ScriptRuntime.ModuleID))
                {
                    _blocked = true;
                    Debug.LogWarning($"[ComponentData] {Name} ignored: another module already updates this data type.");
                    return;
                }
                _writeType = write;
                _claimed = true;
            }

            ulong* owners = null;
            uint count = 0;
            void* dense = ComponentData.Native.GetDense(engine, _writeType, &owners, &count);
            if (count == 0 || dense == null)
                return;
            int n = (int)count;

            ReadTypeIds(_readTypes);
            for (int r = 0; r < _readTypes.Length; r++)
            {
                if (ReadSlots[r].Length < n)
                    ReadSlots[r] = new int[Math.Max(n, ReadSlots[r].Length * 2)];
                ulong* readOwners = null;
                uint readCount = 0;
                ReadDense[r] = (nint)ComponentData.Native.GetDense(engine, _readTypes[r], &readOwners, &readCount);
                fixed (int* slots = ReadSlots[r])
                    ComponentData.Native.Join(engine, _writeType, _readTypes[r], slots, n);
                if (_readTypes[r] == 0 || ReadDense[r] == 0)
                    ReadSlots[r].AsSpan(0, n).Fill(-1);
            }

            int rangeCount = n < ParallelThreshold || !Jobs.IsAvailable ? 1 : (n + RangeSize - 1) / RangeSize;
            while (_ranges.Count < rangeCount)
                _ranges.Add(new Range(this));
            ScriptThread.InDataPass = true;
            try
            {
                if (rangeCount == 1)
                {
                    _ranges[0].Set((nint)dense, (nint)owners, 0, n, dt);
                    _ranges[0].Run();
                }
                else
                {
                    _jobs.Clear();
                    for (int i = 0; i < rangeCount; i++)
                    {
                        int start = i * RangeSize;
                        _ranges[i].Set((nint)dense, (nint)owners, start, Math.Min(start + RangeSize, n), dt);
                        _jobs.Add(Jobs.Schedule(_ranges[i].Action));
                    }
                    Jobs.WaitAll(_jobs);
                }
            }
            finally
            {
                ScriptThread.InDataPass = false;
            }
            for (int i = 0; i < rangeCount; i++)
                _ranges[i].Commands.Playback();
            ComponentData.Native.MarkAllChanged(engine, _writeType);
        }

        private void Report(Exception e)
        {
            lock (_ranges)
            {
                if (_failed)
                    return;
                _failed = true;
            }
            Debug.LogError($"[ComponentData] {Name} threw (reported once): {e}");
        }

        private sealed class Range
        {
            private readonly DataUpdateGroup _group;
            private nint _dense;
            private nint _owners;
            private int _start;
            private int _end;
            private float _dt;

            public Range(DataUpdateGroup group)
            {
                _group = group;
                Action = Run;
            }

            public CommandBuffer Commands { get; } = new();
            public Action Action { get; }

            public void Set(nint dense, nint owners, int start, int end, float dt)
            {
                _dense = dense;
                _owners = owners;
                _start = start;
                _end = end;
                _dt = dt;
            }

            public void Run()
            {
                try
                {
                    _group.RunRange((void*)_dense, (ulong*)_owners, _start, _end, _dt, Commands);
                }
                catch (Exception e)
                {
                    _group.Report(e);
                }
            }
        }
    }

    internal sealed unsafe class DataUpdateGroup<TSelf, TWrite> : DataUpdateGroup
        where TSelf : IComponentDataUpdate<TSelf, TWrite>
        where TWrite : unmanaged, IComponentData
    {
        public static readonly DataUpdateGroup<TSelf, TWrite> Instance = new();

        private DataUpdateGroup() : base(0) { }

        protected override string Name => $"{typeof(TSelf).Name}.OnDataUpdate({typeof(TWrite).Name})";
        protected override uint WriteTypeId => ComponentDataType<TWrite>.Id;
        protected override bool WriteEngineOwned => ComponentDataType<TWrite>.IsEngineOwned;
        protected override void ReadTypeIds(uint[] ids) { }

        protected override void RunRange(void* write, ulong* owners, int start, int end, float dt, CommandBuffer commands)
        {
            TWrite* w = (TWrite*)write;
            for (int i = start; i < end; i++)
                TSelf.OnDataUpdate(ref w[i], new DataUpdateContext(dt, owners[i], commands));
        }
    }

    internal sealed unsafe class DataUpdateGroup<TSelf, TWrite, TRead1> : DataUpdateGroup
        where TSelf : IComponentDataUpdate<TSelf, TWrite, TRead1>
        where TWrite : unmanaged, IComponentData
        where TRead1 : unmanaged, IComponentData
    {
        public static readonly DataUpdateGroup<TSelf, TWrite, TRead1> Instance = new();

        private DataUpdateGroup() : base(1) { }

        protected override string Name => $"{typeof(TSelf).Name}.OnDataUpdate({typeof(TWrite).Name})";
        protected override uint WriteTypeId => ComponentDataType<TWrite>.Id;
        protected override bool WriteEngineOwned => ComponentDataType<TWrite>.IsEngineOwned;
        protected override void ReadTypeIds(uint[] ids) => ids[0] = ComponentDataType<TRead1>.Id;

        protected override void RunRange(void* write, ulong* owners, int start, int end, float dt, CommandBuffer commands)
        {
            TWrite* w = (TWrite*)write;
            TRead1* r1 = (TRead1*)ReadDense[0];
            int[] s1 = ReadSlots[0];
            for (int i = start; i < end; i++)
            {
                int a = s1[i];
                if (a < 0)
                    continue;
                TSelf.OnDataUpdate(ref w[i], in r1[a], new DataUpdateContext(dt, owners[i], commands));
            }
        }
    }

    internal sealed unsafe class DataUpdateGroup<TSelf, TWrite, TRead1, TRead2> : DataUpdateGroup
        where TSelf : IComponentDataUpdate<TSelf, TWrite, TRead1, TRead2>
        where TWrite : unmanaged, IComponentData
        where TRead1 : unmanaged, IComponentData
        where TRead2 : unmanaged, IComponentData
    {
        public static readonly DataUpdateGroup<TSelf, TWrite, TRead1, TRead2> Instance = new();

        private DataUpdateGroup() : base(2) { }

        protected override string Name => $"{typeof(TSelf).Name}.OnDataUpdate({typeof(TWrite).Name})";
        protected override uint WriteTypeId => ComponentDataType<TWrite>.Id;
        protected override bool WriteEngineOwned => ComponentDataType<TWrite>.IsEngineOwned;

        protected override void ReadTypeIds(uint[] ids)
        {
            ids[0] = ComponentDataType<TRead1>.Id;
            ids[1] = ComponentDataType<TRead2>.Id;
        }

        protected override void RunRange(void* write, ulong* owners, int start, int end, float dt, CommandBuffer commands)
        {
            TWrite* w = (TWrite*)write;
            TRead1* r1 = (TRead1*)ReadDense[0];
            TRead2* r2 = (TRead2*)ReadDense[1];
            int[] s1 = ReadSlots[0];
            int[] s2 = ReadSlots[1];
            for (int i = start; i < end; i++)
            {
                int a = s1[i];
                int b = s2[i];
                if (a < 0 || b < 0)
                    continue;
                TSelf.OnDataUpdate(ref w[i], in r1[a], in r2[b], new DataUpdateContext(dt, owners[i], commands));
            }
        }
    }

    internal sealed unsafe class DataUpdateGroup<TSelf, TWrite, TRead1, TRead2, TRead3> : DataUpdateGroup
        where TSelf : IComponentDataUpdate<TSelf, TWrite, TRead1, TRead2, TRead3>
        where TWrite : unmanaged, IComponentData
        where TRead1 : unmanaged, IComponentData
        where TRead2 : unmanaged, IComponentData
        where TRead3 : unmanaged, IComponentData
    {
        public static readonly DataUpdateGroup<TSelf, TWrite, TRead1, TRead2, TRead3> Instance = new();

        private DataUpdateGroup() : base(3) { }

        protected override string Name => $"{typeof(TSelf).Name}.OnDataUpdate({typeof(TWrite).Name})";
        protected override uint WriteTypeId => ComponentDataType<TWrite>.Id;
        protected override bool WriteEngineOwned => ComponentDataType<TWrite>.IsEngineOwned;

        protected override void ReadTypeIds(uint[] ids)
        {
            ids[0] = ComponentDataType<TRead1>.Id;
            ids[1] = ComponentDataType<TRead2>.Id;
            ids[2] = ComponentDataType<TRead3>.Id;
        }

        protected override void RunRange(void* write, ulong* owners, int start, int end, float dt, CommandBuffer commands)
        {
            TWrite* w = (TWrite*)write;
            TRead1* r1 = (TRead1*)ReadDense[0];
            TRead2* r2 = (TRead2*)ReadDense[1];
            TRead3* r3 = (TRead3*)ReadDense[2];
            int[] s1 = ReadSlots[0];
            int[] s2 = ReadSlots[1];
            int[] s3 = ReadSlots[2];
            for (int i = start; i < end; i++)
            {
                int a = s1[i];
                int b = s2[i];
                int c = s3[i];
                if (a < 0 || b < 0 || c < 0)
                    continue;
                TSelf.OnDataUpdate(ref w[i], in r1[a], in r2[b], in r3[c], new DataUpdateContext(dt, owners[i], commands));
            }
        }
    }
}
