// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Runtime.CompilerServices;

namespace xg
{
    public interface IComponentDataHandle
    {
        IntPtr NativePtr { get; }

        void Refresh();
        void Commit();
    }

    public unsafe interface IComponentUpdate<T> : IComponentUpdate
        where T : IComponentUpdate<T>
    {
        static IComponentUpdate()
        {
            Type realType = typeof(T);
            if (!typeof(IComponentUpdate<T>).IsAssignableFrom(realType))
            {
                throw new InvalidOperationException(
                    $"Type {realType.Name} must implement IComponentUpdate<{realType.Name}>"
                );
            }
        }

        protected static delegate* managed<T, float, void> UpdateFunc { get; set; }
        static abstract void OnUpdate(T component, float deltaTime);
        void IComponentUpdate.Setup()
        {
            UpdateFunc = &T.OnUpdate;
        }

        void IComponentUpdate.Update(float deltaTime)
        {
            UpdateFunc((T)this, deltaTime);
        }
    }


    public abstract class ComponentOLD
    {
        public abstract int DataSize { get; }
        public abstract void Refresh();
        public abstract void Commit();

        internal abstract void BindHandle(IComponentDataHandle r);
    }

    public interface IComponentDataHandler
    {

    }





    public abstract class Component<TData> : ComponentOLD
        where TData : struct, IComponentData
    {
        protected TData _data = default;

        public ref TData Data => ref _data;
        public TData DataCopy => _data;

        // Called once when the component is added
        public virtual void OnStart() { }

        internal Component()
        {
            BlittableValidator.Validate<TData>();
        }

        internal IComponentDataHandle? _handle;

        internal override void BindHandle(IComponentDataHandle r)
        {
            _handle = r;
            // When bound, refresh from native memory so Data is current
            _handle.Refresh();
        }

        public override int DataSize => ComponentStrideCache.GetOrCompute<TData>();
        public IEditScope? BeginEdit(bool refreshFirst = true)
        {
            if (refreshFirst) Refresh();
            return (IEditScope?)_handle;
        }

        // Refresh: copy bytes from native memory into managed struct (instance.Data)
        public unsafe override void Refresh()
        {
            var ptr = _handle?.NativePtr ?? IntPtr.Zero;
            if (ptr == IntPtr.Zero) return;

            byte* src = (byte*)ptr.ToPointer();
            TData local = default;
            void* destPtr = Unsafe.AsPointer(ref local);
            Buffer.MemoryCopy(src, destPtr, DataSize, DataSize);
            _data = local;
        }

        // Commit: copy managed struct bytes into native memory
        public unsafe override void Commit()
        {
            var ptr = _handle?.NativePtr ?? IntPtr.Zero;
            if (ptr == IntPtr.Zero) return;

            byte* dest = (byte*)ptr.ToPointer();
            TData local = _data;
            void* srcPtr = Unsafe.AsPointer(ref local);
            Buffer.MemoryCopy(srcPtr, dest, DataSize, DataSize);
        }
    }
    public sealed class ComponentDataHandle<T> : IComponentDataHandle, IEditScope
                                        where T : ComponentOLD, new()
    {
        private readonly IntPtr _nativePtr;
        private T? _instance;
        public bool AutoCommit = true;

        // IMPORTANT: stride is based on the payload struct D
        public IntPtr NativePtr => _nativePtr;

        internal ComponentDataHandle(IntPtr ptr)
        {
            if (ptr == IntPtr.Zero) throw new ArgumentException("native pointer is zero", nameof(ptr));
            _nativePtr = ptr;
        }

        /// <summary>
        /// Create or return the managed façade instance bound to this native slot.
        /// The façade is constructed with new() and immediately bound (Refresh is called).
        /// </summary>
        public T GetOrCreate(bool autoCommit = true)
        {
            AutoCommit = autoCommit;
            if (_instance == null)
            {
                _instance = new T();
                _instance.BindHandle(this);
            }
            return _instance;
        }

        public static implicit operator T(ComponentDataHandle<T> r) => r.GetOrCreate();

        /// <summary>
        /// Bind an existing managed façade instance to this native slot.
        /// This sets the back-reference and immediately refreshes the managed Data from native memory.
        /// </summary>
        public void Bind(T instance)
        {
            ArgumentNullException.ThrowIfNull(instance);
            _instance = instance;
            instance.BindHandle(this);
        }

        // IEditScope.Dispose -> commit if AutoCommit
        public void Dispose()
        {
            if (AutoCommit)
                _instance?.Commit();
        }

        public void Refresh()
        {
            _instance?.Refresh();
        }

        public void Commit()
        {
            _instance?.Commit();
        }
    }
    public sealed class ComponentHandle<T> : IComponentDataHandle, IEditScope
                                        where T : ComponentOLD, new()
    {
        private readonly IntPtr _nativePtr;
        private T? _instance;
        public bool AutoCommit = true;

        // IMPORTANT: stride is based on the payload struct D
        public IntPtr NativePtr => _nativePtr;

        internal ComponentHandle(IntPtr ptr)
        {
            if (ptr == IntPtr.Zero) throw new ArgumentException("native pointer is zero", nameof(ptr));
            _nativePtr = ptr;
        }

        /// <summary>
        /// Create or return the managed façade instance bound to this native slot.
        /// The façade is constructed with new() and immediately bound (Refresh is called).
        /// </summary>
        public T GetOrCreate(bool autoCommit = true)
        {
            AutoCommit = autoCommit;
            if (_instance == null)
            {
                _instance = new T();
                _instance.BindHandle(this);
            }
            return _instance;
        }

        public static implicit operator T(ComponentHandle<T> r) => r.GetOrCreate();

        /// <summary>
        /// Bind an existing managed façade instance to this native slot.
        /// This sets the back-reference and immediately refreshes the managed Data from native memory.
        /// </summary>
        public void Bind(T instance)
        {
            ArgumentNullException.ThrowIfNull(instance);
            _instance = instance;
            instance.BindHandle(this);
        }

        // IEditScope.Dispose -> commit if AutoCommit
        public void Dispose()
        {
            if (AutoCommit)
                _instance?.Commit();
        }

        public void Refresh()
        {
            _instance?.Refresh();
        }

        public void Commit()
        {
            _instance?.Commit();
        }
    }

}


