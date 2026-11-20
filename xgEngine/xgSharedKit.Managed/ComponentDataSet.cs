// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

namespace xg
{
    // A component with native data: each declared struct is an element in that type's tightly packed engine pool, added
    // when the component attaches and released when it detaches (one reference per component, so components declaring
    // the same struct share it). DataN returns a reference straight into the pool: no copies, nothing to commit.
    // Implement IComponentDataUpdate<TSelf, TData, ...> to update every element of a declared type ECS-style.
    public abstract unsafe class ComponentDataSet : Component
    {
        // The (data type id, key) pairs this component holds; used by hot reload to keep elements across the unload.
        internal abstract IEnumerable<(uint Type, string Key)> DataTypes { get; }

        public abstract int DataCount { get; }
        public abstract Type DataType(int index);
        // The element as a boxed copy (Inspector), and writing a boxed value back.
        public abstract object? GetData(int index);
        public abstract void SetData(int index, object value);

        internal override void AttachInternal(Actor actor)
        {
            foreach ((uint type, _) in DataTypes)
                if (type == 0 || ComponentData.Native.Add(ScriptRuntime._enginePtr, type, actor.ID) == null)
                    Debug.LogError($"[ComponentData] {GetType().Name}: could not add data to actor {(ulong)actor.ID}.");
            RegisterUpdates();
        }

        internal override void DetachInternal(Actor actor)
        {
            if (GameScriptHost.IsUnloading)
                return;
            foreach ((uint type, _) in DataTypes)
                ComponentData.Native.Release(ScriptRuntime._enginePtr, type, actor.ID);
        }

        private protected abstract void RegisterUpdates();

        private protected static unsafe ref T Element<T>(ActorID actor) where T : unmanaged, IComponentData
        {
            T* p = ComponentData.Pointer<T>(actor);
            if (p == null)
                throw new InvalidOperationException($"{typeof(T).Name} of actor {(ulong)actor} is not available (component detached?).");
            return ref *p;
        }

        private protected static unsafe object? Box<T>(ActorID actor) where T : unmanaged, IComponentData
        {
            T* p = ComponentData.Pointer<T>(actor);
            return p == null ? null : *p;
        }

        private protected static unsafe void Unbox<T>(ActorID actor, object value) where T : unmanaged, IComponentData
        {
            T* p = ComponentData.Pointer<T>(actor);
            if (p != null && value is T typed)
                *p = typed;
        }

        private protected static (uint, string) Entry<T>() where T : unmanaged, IComponentData =>
            (ComponentDataType<T>.Id, ComponentDataType<T>.Key);
    }

    public abstract class ComponentDataSet<T1> : ComponentDataSet
        where T1 : unmanaged, IComponentData
    {
        public ref T1 Data1 => ref Element<T1>(ActorID);

        internal override IEnumerable<(uint Type, string Key)> DataTypes
        {
            get { yield return Entry<T1>(); }
        }

        public override int DataCount => 1;
        public override Type DataType(int index) => typeof(T1);
        public override object? GetData(int index) => Box<T1>(ActorID);
        public override void SetData(int index, object value) => Unbox<T1>(ActorID, value);

        private protected override void RegisterUpdates()
        {
            if (this is IDataUpdateRegistrar<T1> r1)
                r1.RegisterDataUpdate();
        }
    }

    public abstract class ComponentDataSet<T1, T2> : ComponentDataSet
        where T1 : unmanaged, IComponentData
        where T2 : unmanaged, IComponentData
    {
        public ref T1 Data1 => ref Element<T1>(ActorID);
        public ref T2 Data2 => ref Element<T2>(ActorID);

        internal override IEnumerable<(uint Type, string Key)> DataTypes
        {
            get { yield return Entry<T1>(); yield return Entry<T2>(); }
        }

        public override int DataCount => 2;
        public override Type DataType(int index) => index == 0 ? typeof(T1) : typeof(T2);
        public override object? GetData(int index) => index == 0 ? Box<T1>(ActorID) : Box<T2>(ActorID);

        public override void SetData(int index, object value)
        {
            if (index == 0) Unbox<T1>(ActorID, value); else Unbox<T2>(ActorID, value);
        }

        private protected override void RegisterUpdates()
        {
            if (this is IDataUpdateRegistrar<T1> r1)
                r1.RegisterDataUpdate();
            if (this is IDataUpdateRegistrar<T2> r2)
                r2.RegisterDataUpdate();
        }
    }

    public abstract class ComponentDataSet<T1, T2, T3> : ComponentDataSet
        where T1 : unmanaged, IComponentData
        where T2 : unmanaged, IComponentData
        where T3 : unmanaged, IComponentData
    {
        public ref T1 Data1 => ref Element<T1>(ActorID);
        public ref T2 Data2 => ref Element<T2>(ActorID);
        public ref T3 Data3 => ref Element<T3>(ActorID);

        internal override IEnumerable<(uint Type, string Key)> DataTypes
        {
            get { yield return Entry<T1>(); yield return Entry<T2>(); yield return Entry<T3>(); }
        }

        public override int DataCount => 3;
        public override Type DataType(int index) => index switch { 0 => typeof(T1), 1 => typeof(T2), _ => typeof(T3) };
        public override object? GetData(int index) => index switch { 0 => Box<T1>(ActorID), 1 => Box<T2>(ActorID), _ => Box<T3>(ActorID) };

        public override void SetData(int index, object value)
        {
            switch (index)
            {
                case 0: Unbox<T1>(ActorID, value); break;
                case 1: Unbox<T2>(ActorID, value); break;
                default: Unbox<T3>(ActorID, value); break;
            }
        }

        private protected override void RegisterUpdates()
        {
            if (this is IDataUpdateRegistrar<T1> r1)
                r1.RegisterDataUpdate();
            if (this is IDataUpdateRegistrar<T2> r2)
                r2.RegisterDataUpdate();
            if (this is IDataUpdateRegistrar<T3> r3)
                r3.RegisterDataUpdate();
        }
    }

    public abstract class ComponentDataSet<T1, T2, T3, T4> : ComponentDataSet
        where T1 : unmanaged, IComponentData
        where T2 : unmanaged, IComponentData
        where T3 : unmanaged, IComponentData
        where T4 : unmanaged, IComponentData
    {
        public ref T1 Data1 => ref Element<T1>(ActorID);
        public ref T2 Data2 => ref Element<T2>(ActorID);
        public ref T3 Data3 => ref Element<T3>(ActorID);
        public ref T4 Data4 => ref Element<T4>(ActorID);

        internal override IEnumerable<(uint Type, string Key)> DataTypes
        {
            get { yield return Entry<T1>(); yield return Entry<T2>(); yield return Entry<T3>(); yield return Entry<T4>(); }
        }

        public override int DataCount => 4;
        public override Type DataType(int index) => index switch { 0 => typeof(T1), 1 => typeof(T2), 2 => typeof(T3), _ => typeof(T4) };
        public override object? GetData(int index) => index switch { 0 => Box<T1>(ActorID), 1 => Box<T2>(ActorID), 2 => Box<T3>(ActorID), _ => Box<T4>(ActorID) };

        public override void SetData(int index, object value)
        {
            switch (index)
            {
                case 0: Unbox<T1>(ActorID, value); break;
                case 1: Unbox<T2>(ActorID, value); break;
                case 2: Unbox<T3>(ActorID, value); break;
                default: Unbox<T4>(ActorID, value); break;
            }
        }

        private protected override void RegisterUpdates()
        {
            if (this is IDataUpdateRegistrar<T1> r1)
                r1.RegisterDataUpdate();
            if (this is IDataUpdateRegistrar<T2> r2)
                r2.RegisterDataUpdate();
            if (this is IDataUpdateRegistrar<T3> r3)
                r3.RegisterDataUpdate();
            if (this is IDataUpdateRegistrar<T4> r4)
                r4.RegisterDataUpdate();
        }
    }
}
