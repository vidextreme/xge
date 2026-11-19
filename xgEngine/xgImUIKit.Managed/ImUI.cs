// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Reflection;
using System.Runtime.InteropServices;
using System.Runtime.Loader;

namespace xg.imui
{
    // Lifetime of everything this assembly registered with xgImUIKit.
    // ShutdownAll runs automatically when the owning load context unloads (hot reload); call it from module shutdown too.
    public static class ImUI
    {
        internal static readonly List<Panel> Panels = new();

        // Every registered managed panel, in registration order.
        public static IReadOnlyList<Panel> RegisteredPanels => Panels;
        private static readonly List<IDisposable> _resources = new();
        private static readonly ShellBarBinding?[] _shellBars = new ShellBarBinding?[2];
        private static bool? _compatible;

        static ImUI()
        {
            AssemblyLoadContext? context = AssemblyLoadContext.GetLoadContext(typeof(ImUI).Assembly);
            if (context is not null && context.IsCollectible)
                context.Unloading += _ => ShutdownAll();
        }

        public static uint NativeApiVersion => Native.GetApiVersion();

        internal static bool EnsureCompatible()
        {
            if (_compatible is bool known)
                return known;
            uint version = Native.GetApiVersion();
            _compatible = version >= Native.ApiVersion;
            if (_compatible != true)
                Debug.LogError($"xgImUIKit API version {version} is older than xgImUIKit.Managed requires ({Native.ApiVersion}); immediate UI disabled.");
            return _compatible.Value;
        }

        // Draws an editor shell bar with action every frame (an implicit row); null clears it.
        public static void SetShellBar(ShellBar bar, Action? action)
        {
            int index = (int)bar;
            _shellBars[index]?.Dispose();
            _shellBars[index] = action is not null && EnsureCompatible() ? new ShellBarBinding(bar, action) : null;
        }

        // Rebuilds the dock layout from the panel slots on the next frame.
        public static void ResetLayout() => Native.ResetLayout();

        internal static void Track(IDisposable resource) => _resources.Add(resource);
        internal static void Untrack(IDisposable resource) => _resources.Remove(resource);

        // Unregisters the panels and menu commands whose code lives in context (scripts that are about to unload).
        public static void UnregisterFrom(AssemblyLoadContext context)
        {
            for (int i = Panels.Count - 1; i >= 0; --i)
                if (AssemblyLoadContext.GetLoadContext(Panels[i].GetType().Assembly) == context)
                    Panels[i].Unregister();
            for (int i = _resources.Count - 1; i >= 0; --i)
                if (_resources[i] is MenuCommand command && command.BelongsTo(context))
                    command.Dispose();
        }

        public static void ShutdownAll()
        {
            for (int i = Panels.Count - 1; i >= 0; --i)
                Panels[i].Unregister();
            for (int i = _resources.Count - 1; i >= 0; --i)
                _resources[i].Dispose();
            Panels.Clear();
            _resources.Clear();
            Array.Clear(_shellBars);
        }

        private sealed unsafe class ShellBarBinding : IDisposable
        {
            private readonly ShellBar _bar;
            private readonly Action _action;
            private GCHandle _self;
            private bool _reported;

            public ShellBarBinding(ShellBar bar, Action action)
            {
                _bar = bar;
                _action = action;
                _self = GCHandle.Alloc(this, GCHandleType.Normal);
                Native.SetShellBar(bar, &Trampoline, GCHandle.ToIntPtr(_self));
                Track(this);
            }

            public void Dispose()
            {
                if (!_self.IsAllocated)
                    return;
                Native.SetShellBar(_bar, null, 0);
                _self.Free();
                Untrack(this);
            }

            [UnmanagedCallersOnly]
            private static void Trampoline(nint ctx, nint user)
            {
                if (GCHandle.FromIntPtr(user).Target is not ShellBarBinding binding)
                    return;
                nint previous = Gui.Enter(ctx);
                try
                {
                    binding._action();
                }
                catch (Exception e)
                {
                    if (!binding._reported)
                        Debug.LogException(e);
                    binding._reported = true;
                }
                finally
                {
                    Gui.Leave(previous);
                }
            }
        }
    }
}
