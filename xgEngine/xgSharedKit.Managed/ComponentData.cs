// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Runtime.CompilerServices;

namespace xg
{
    public abstract class ComponentData
    {
        public abstract int DataSize { get; }
        public abstract void Refresh();
        public abstract void Commit();

        internal abstract void BindHandle(IComponentDataHandle r);
    }

    public abstract class ComponentData<TData> : ComponentData
        where TData : struct, IComponentData
    {
        protected IntPtr NativePtr = IntPtr.Zero;
        protected TData _data = default;

        public ref TData Data => ref _data;
        public TData DataCopy => _data;

        internal ComponentData()
        {
            BlittableValidator.Validate<TData>();
        }

        internal override void BindHandle(IComponentDataHandle r)
        {
            //_handle = r;
            //_handle.Refresh();
        }

        public override int DataSize => ComponentStrideCache.GetOrCompute<TData>();
        public IEditScope BeginEdit(bool refreshFirst = true)
        {
            if (refreshFirst) Refresh();
            return new EditScope(this);
        }

        public unsafe override void Refresh()
        {
            var ptr = NativePtr;
            if (ptr == IntPtr.Zero) return;

            byte* src = (byte*)ptr.ToPointer();
            TData local = default;
            void* destPtr = Unsafe.AsPointer(ref local);
            Buffer.MemoryCopy(src, destPtr, DataSize, DataSize);
            _data = local;
        }

        public unsafe override void Commit()
        {
            var ptr = NativePtr;
            if (ptr == IntPtr.Zero) return;

            byte* dest = (byte*)ptr.ToPointer();
            TData local = _data;
            void* srcPtr = Unsafe.AsPointer(ref local);
            Buffer.MemoryCopy(srcPtr, dest, DataSize, DataSize);
        }
    }

    public interface IComponentDataUpdate<TData> where TData : struct, IComponentData
    {
        static abstract void OnDataUpdate(ComponentData<TData> comp, float dt);
    }
}
