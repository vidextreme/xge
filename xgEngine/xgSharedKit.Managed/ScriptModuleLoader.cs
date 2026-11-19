// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Diagnostics;
using System.Reflection;
using System.Runtime.CompilerServices;

namespace xg
{
    // A script module whose assembly lives in its own collectible ScriptLoadContext (see ScriptModuleLoader).
    public sealed class LoadedScriptModule
    {
        internal LoadedScriptModule(string id, ScriptLoadContext context, Assembly assembly, byte[] image, byte[]? symbols)
        {
            Id = id;
            Context = context;
            Assembly = assembly;
            Image = image;
            Symbols = symbols;
        }

        public string Id { get; }
        public ScriptLoadContext Context { get; }
        public Assembly Assembly { get; }
        // The bytes it was loaded from, kept so a failed reload can load this build again.
        public byte[] Image { get; }
        public byte[]? Symbols { get; }
    }

    // Loads a game project's script assembly (with a generated <Assembly>.ScriptEntry) into a collectible load context
    // and adds it to the engine as a function-pointer module, and unloads it again: the base of script hot reload.
    // Main thread only, and never inside a module update (use ScriptRuntime.RunBetweenFrames).
    public static unsafe partial class ScriptModuleLoader
    {
        private const string NativeLib = "xgScriptNative";

        [StructLayout(LayoutKind.Sequential)]
        private struct ModuleFunctions
        {
            public uint Size;
            public nint Init;
            public nint Update;
            public nint Shutdown;
            public nint OnMessage;
        }

        // Loads the assembly at path (its folder supplies its dependencies) as module id. state: a ReloadState capture
        // to restore (a reload), or null for a first load. Returns null when loading or the module's Init fails.
        public static LoadedScriptModule? Load(string id, string path, string? state = null, string? parentId = null, string? group = null)
        {
            string pdb = Path.ChangeExtension(path, ".pdb");
            return Load(id, Path.GetDirectoryName(path)!, File.ReadAllBytes(path), File.Exists(pdb) ? File.ReadAllBytes(pdb) : null, state, parentId, group);
        }

        public static LoadedScriptModule? Load(string id, string folder, byte[] image, byte[]? symbols, string? state = null,
            string? parentId = null, string? group = null)
        {
            ScriptLoadContext context = new(id, folder);
            try
            {
                Assembly assembly = context.LoadBytes(image, symbols);
                string entryName = $"{assembly.GetName().Name}.ScriptEntry";
                Type entry = assembly.GetType(entryName, true)!;
                ModuleFunctions functions = new()
                {
                    Size = (uint)sizeof(ModuleFunctions),
                    Init = Pointer(entry, "Init"),
                    Update = Pointer(entry, "Update"),
                    Shutdown = Pointer(entry, "Shutdown"),
                    OnMessage = entry.GetMethod("OnMessage") is null ? 0 : Pointer(entry, "OnMessage"),
                };
                SetHostFlag(assembly, "IsReload", state is not null);
                if (!XG_AddFunctionModule(ScriptRuntime._enginePtr, ScriptRuntime.ModuleID, id, parentId, group, &functions))
                {
                    context.Unload();
                    return null;
                }
                LoadedScriptModule module = new(id, context, assembly, image, symbols);
                if (state is not null)
                    CallState(module, "Restore", state);
                return module;
            }
            catch (Exception e)
            {
                Debug.LogError($"[Scripts] Could not load {id}: {(e as TargetInvocationException)?.InnerException?.Message ?? e.Message}");
                context.Unload();
                return null;
            }
        }

        // Captures the module's actors and component values (ReloadState) when capture is set, removes the module from
        // the engine (its [GameUnload] methods run instead of [GameShutdown] when capturing) and unloads its context.
        // Clears module, so this call holds the last reference. Returns the capture, or null.
        public static string? Unload(ref LoadedScriptModule? module, bool capture)
        {
            if (module is null)
                return null;
            string id = module.Id;
            (string? state, WeakReference context) = RemoveAndRelease(module, capture);
            module = null;
            WaitForUnload(context, $"{id} scripts");
            return state;
        }

        private static readonly List<(WeakReference Context, string What, int Checks)> _lingering = new();

        // Collects until context (a weak reference to an unloaded AssemblyLoadContext) is gone and logs it. A context
        // still alive is checked again by CheckLingering on later frames (references from the reload's own frame
        // usually clear by then).
        public static bool WaitForUnload(WeakReference context, string what)
        {
            Stopwatch clock = Stopwatch.StartNew();
            for (int i = 0; context.IsAlive && i < 10; i++)
            {
                GC.Collect();
                GC.WaitForPendingFinalizers();
            }
            if (context.IsAlive)
            {
                _lingering.Add((context, what, 0));
                return false;
            }
            Debug.Log($"[Scripts] Previous {what} unloaded ({clock.Elapsed.TotalMilliseconds:F0} ms)");
            return true;
        }

        // Call now and then (for example every second) after a reload: reports contexts that finally unloaded, and
        // warns about ones that are still alive after several checks.
        public static void CheckLingering()
        {
            if (_lingering.Count == 0)
                return;
            GC.Collect();
            GC.WaitForPendingFinalizers();
            GC.Collect();
            for (int i = _lingering.Count - 1; i >= 0; --i)
            {
                (WeakReference context, string what, int checks) = _lingering[i];
                if (!context.IsAlive)
                {
                    Debug.Log($"[Scripts] Previous {what} unloaded");
                    _lingering.RemoveAt(i);
                }
                else if (checks + 1 >= 5)
                {
                    Debug.LogWarning($"[Scripts] The previous {what} are still referenced and stay in memory (something kept one of their objects or types).");
                    _lingering.RemoveAt(i);
                }
                else
                {
                    _lingering[i] = (context, what, checks + 1);
                }
            }
        }

        [MethodImpl(MethodImplOptions.NoInlining)]
        private static (string? State, WeakReference Context) RemoveAndRelease(LoadedScriptModule module, bool capture)
        {
            string? state = null;
            if (capture)
            {
                state = CallState(module, "Capture", null) as string;
                SetHostFlag(module.Assembly, "IsUnloading", true);
            }
            XG_RemoveScriptModule(ScriptRuntime._enginePtr, module.Id);
            module.Context.Unload();
            return (state, new WeakReference(module.Context));
        }

        // Calls xg.ReloadState.<method>(argument) in the module's own copy of the shared kit.
        private static object? CallState(LoadedScriptModule module, string method, string? argument)
        {
            Type? type = SharedKitType(module.Assembly, "xg.ReloadState");
            MethodInfo? call = type?.GetMethod(method, BindingFlags.Public | BindingFlags.Static);
            if (call is null)
            {
                Debug.LogWarning($"[Scripts] {module.Id}: its shared kit has no ReloadState.{method}; component values are not kept.");
                return null;
            }
            try
            {
                return call.Invoke(null, argument is null ? null : new object[] { argument });
            }
            catch (TargetInvocationException e)
            {
                Debug.LogError($"[Scripts] {module.Id}: ReloadState.{method} failed: {e.InnerException?.Message ?? e.Message}");
                return null;
            }
        }

        private static void SetHostFlag(Assembly scripts, string property, bool value)
        {
            Type? host = SharedKitType(scripts, "xg.GameScriptHost");
            host?.GetProperty(property, BindingFlags.Public | BindingFlags.Static)?.SetValue(null, value);
        }

        // A type of the shared kit as the scripts' own load context sees it.
        private static Type? SharedKitType(Assembly scripts, string fullName)
        {
            string shared = typeof(ScriptModuleLoader).Assembly.GetName().Name!;
            foreach (AssemblyName reference in scripts.GetReferencedAssemblies())
            {
                if (reference.Name != shared)
                    continue;
                System.Runtime.Loader.AssemblyLoadContext? context = System.Runtime.Loader.AssemblyLoadContext.GetLoadContext(scripts);
                Assembly? kit = context?.LoadFromAssemblyName(reference);
                return kit?.GetType(fullName, false);
            }
            return null;
        }

        private static nint Pointer(Type entry, string name)
        {
            MethodInfo method = entry.GetMethod(name, BindingFlags.Public | BindingFlags.Static)
                ?? throw new MissingMethodException(entry.FullName, name);
            return method.MethodHandle.GetFunctionPointer();
        }

        [LibraryImport(NativeLib, EntryPoint = "XG_AddFunctionModule", StringMarshalling = StringMarshalling.Utf8)]
        [return: MarshalAs(UnmanagedType.U1)]
        private static partial bool XG_AddFunctionModule(nint engine, uint callerModuleId, string id, string? parentId, string? group, ModuleFunctions* functions);

        [LibraryImport(NativeLib, EntryPoint = "XG_RemoveScriptModule", StringMarshalling = StringMarshalling.Utf8)]
        [return: MarshalAs(UnmanagedType.U1)]
        private static partial bool XG_RemoveScriptModule(nint engine, string id);
    }
}
