using System;
using System.Runtime.InteropServices;

namespace xg
{
    //
    // ComponentRef<T>
    //
    // Stable managed reference to a native component block.
    // T may be a managed struct or class.
    //
    public sealed class ComponentRef<T>
    {
        private IntPtr _nativePtr;

        internal ComponentRef(IntPtr ptr)
        {
            _nativePtr = ptr;
        }

        internal void UpdatePointer(IntPtr newPtr)
        {
            _nativePtr = newPtr;
        }

        public bool IsValid => _nativePtr != IntPtr.Zero;

        //
        // Safe value copy from native memory.
        // T may be managed or unmanaged.
        //
        public T Read()
        {
            if (!IsValid)
                throw new InvalidOperationException("ComponentRef is invalid.");

            return Marshal.PtrToStructure<T>(_nativePtr);
        }

        //
        // Safe write back to native memory.
        //
        public void Write(T value)
        {
            if (!IsValid)
                throw new InvalidOperationException("ComponentRef is invalid.");

            Marshal.StructureToPtr(value, _nativePtr, false);
        }
    }
}
