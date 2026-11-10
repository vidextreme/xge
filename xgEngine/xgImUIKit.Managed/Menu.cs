// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Runtime.InteropServices;

namespace xg.imui
{
    // Main menu commands. Path "Tools/Rebuild" adds a Tools menu when it does not exist yet.
    public static class Menu
    {
        public static MenuCommand Add(string path, Action action, string? shortcut = null) => new(path, shortcut, action);
    }

    public sealed unsafe class MenuCommand : IDisposable
    {
        private nint _handle;
        private GCHandle _self;
        private readonly Action _action;

        internal MenuCommand(string path, string? shortcut, Action action)
        {
            _action = action ?? throw new ArgumentNullException(nameof(action));
            if (!ImUI.EnsureCompatible())
                return;
            _self = GCHandle.Alloc(this, GCHandleType.Normal);
            fixed (byte* p = Utf8.Encode(path.AsSpan(), 0))
            fixed (byte* s = Utf8.Encode(shortcut, 1))
                _handle = Native.RegisterMenuItem(p, s, &InvokeTrampoline, GCHandle.ToIntPtr(_self));
            if (_handle == 0)
            {
                _self.Free();
                return;
            }
            ImUI.Track(this);
        }

        public bool IsRegistered => _handle != 0;

        public void Dispose()
        {
            if (_handle == 0)
                return;
            Native.UnregisterMenuItem(_handle);
            _handle = 0;
            if (_self.IsAllocated)
                _self.Free();
            ImUI.Untrack(this);
        }

        [UnmanagedCallersOnly]
        private static void InvokeTrampoline(nint user)
        {
            try
            {
                (GCHandle.FromIntPtr(user).Target as MenuCommand)?._action();
            }
            catch (Exception e)
            {
                Debug.LogException(e);
            }
        }
    }
}
