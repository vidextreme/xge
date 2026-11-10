// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Reflection;
using System.Runtime.Loader;

namespace xg.imui
{
    // Lifetime of everything this assembly registered with xgImUIKit.
    // ShutdownAll runs automatically when the owning load context unloads (hot reload); call it from module shutdown too.
    public static class ImUI
    {
        internal static readonly List<Panel> Panels = new();
        private static readonly List<IDisposable> s_resources = new();
        private static bool? s_compatible;

        static ImUI()
        {
            AssemblyLoadContext? context = AssemblyLoadContext.GetLoadContext(typeof(ImUI).Assembly);
            if (context is not null && context.IsCollectible)
                context.Unloading += _ => ShutdownAll();
        }

        public static uint NativeApiVersion => Native.GetApiVersion();

        internal static bool EnsureCompatible()
        {
            if (s_compatible is bool known)
                return known;
            uint version = Native.GetApiVersion();
            s_compatible = version >= Native.ApiVersion;
            if (s_compatible != true)
                Debug.LogError($"xgImUIKit API version {version} is older than xgImUIKit.Managed requires ({Native.ApiVersion}); immediate UI disabled.");
            return s_compatible.Value;
        }

        internal static void Track(IDisposable resource) => s_resources.Add(resource);
        internal static void Untrack(IDisposable resource) => s_resources.Remove(resource);

        public static void ShutdownAll()
        {
            for (int i = Panels.Count - 1; i >= 0; --i)
                Panels[i].Unregister();
            for (int i = s_resources.Count - 1; i >= 0; --i)
                s_resources[i].Dispose();
            Panels.Clear();
            s_resources.Clear();
        }
    }
}
