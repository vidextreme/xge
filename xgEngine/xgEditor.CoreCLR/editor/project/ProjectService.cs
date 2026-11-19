// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Collections.Concurrent;
using System.Diagnostics;
using System.Reflection;
using System.Runtime.Loader;
using System.Text;
using System.Text.RegularExpressions;
using xg;
using xg.editor;
using xg.imui;
using Debug = xg.Debug;

namespace xgEditor.CoreCLR
{
    // The open game project: created (xge.exe --new-project) or loaded (--project / gameconfig ProjectPath) when the
    // editor starts, with its generated files kept current and its scripts rebuilt when stale, plus the Assets menu
    // commands that act on it. Open runs before the kernel loads the project's script module, so a build done there is
    // loaded in the same session. Later builds (Assets > Build Scripts, the toolbar, or a saved script) hot reload the
    // scripts: game.project and the editor scripts live in collectible load contexts and are swapped between frames,
    // keeping the actors and their component values.
    public static partial class ProjectService
    {
        private const int BuildTimeoutMilliseconds = 120_000;

        // Script module id of the open project's scripts.
        public const string ProjectModuleId = "game.project";

        private static int _building;
        private static readonly TimeSpan WatchQuiet = TimeSpan.FromSeconds(1.0);
        private static readonly ConcurrentQueue<Action> _mainThread = new();
        private static LoadedScriptModule? _scripts;
        private static ScriptLoadContext? _editorContext;
        private static Assembly? _editorAssembly;
        private static ScriptWatcher? _watcher;
        private static int _testReloads;
        private static int _testFrames;
        private static int _frames;

        public static ProjectInfo? Current { get; private set; }
        // The open project's assets (meta files, tags, bookmarks); null without a project.
        public static AssetDatabase? Assets { get; private set; }
        public static bool IsBuilding => Volatile.Read(ref _building) != 0;

        public static void Open()
        {
            string path = ScriptRuntime.ProjectPath;
            if (path.Length == 0)
                return;

            Current = ScriptRuntime.NewProjectPath.Length > 0 ? ProjectFiles.Create(path) : ProjectInfo.Load(path);
            if (Current is null)
            {
                Debug.LogError($"[Project] No {ProjectInfo.ManifestExtension} project at {path}");
                return;
            }

            Startup.Report($"Opening {Current.Name}", Current.Root, Startup.Indeterminate);
            ProjectFiles.Generate(Current);
            Menu.Add("Assets/Open C# Project", OpenCSharpProject);
            Menu.Add("Assets/Build Scripts", () => BuildScripts());
            Menu.Add("Assets/Refresh", () => Assets?.Refresh());
            Debug.Log($"[Project] Opened {Current.Name} ({Current.Root})");
            Startup.Report("Indexing assets", Current.Root, Startup.Indeterminate);
            Assets = new AssetDatabase(Current.Root);
            AddResourceSource(Current);

            if (ScriptRuntime.ProjectAutoBuild)
            {
                string? stale = ScriptsStaleReason(Current);
                if (stale is null)
                    Debug.Log($"[Project] {Current.Name} scripts are up to date");
                else
                    BuildNow(Current, stale);
            }
            LoadScriptModule(Current);
            LoadEditorAssembly(Current);
            _watcher = new ScriptWatcher(Current);
            if (int.TryParse(Environment.GetEnvironmentVariable("XG_SCRIPT_TEST_RELOAD"), out int reloads))
                _testReloads = reloads;
        }

        // Per frame (main thread): runs work queued by background builds, starts a build once saved scripts have been
        // quiet for a moment, and drives the XG_SCRIPT_TEST_RELOAD hook.
        public static void Update()
        {
            while (_mainThread.TryDequeue(out Action? action))
                action();
            if (Current is not null && _watcher is not null && !IsBuilding && _watcher.TakeSettled(WatchQuiet))
            {
                Debug.Log("[Scripts] Script files changed; rebuilding");
                _ = BuildScripts();
            }
            if (++_frames % 60 == 0)
                ScriptModuleLoader.CheckLingering();
            if (_testReloads > 0 && ++_testFrames % 120 == 0)
            {
                _testReloads--;
                ScriptReloadTest.BeforeReload();
                RequestReload();
            }
        }

        public static void Shutdown()
        {
            _watcher?.Dispose();
            _watcher = null;
        }

        // Swaps in the current build of the project's scripts between frames.
        public static void RequestReload()
        {
            ProjectInfo? project = Current;
            if (project is not null)
                ScriptRuntime.RunBetweenFrames(() => Reload(project));
        }

        // Runs between frames: captures the actors and component values, unloads the editor scripts and game.project,
        // loads the new build and restores them. A build that fails to load is replaced by the previous one again.
        private static void Reload(ProjectInfo project)
        {
            if (!File.Exists(project.ScriptAssemblyPath))
                return;
            Stopwatch clock = Stopwatch.StartNew();
            byte[]? previousImage = _scripts?.Image;
            byte[]? previousSymbols = _scripts?.Symbols;

            DrawerRegistry.ClearInstances();
            TypeBridge.Clear();
            ComponentIcons.Clear();
            ComponentCatalog.Invalidate();
            WeakReference? editorContext = UnloadEditorAssembly();
            Selection.Refresh();
            string state = ScriptModuleLoader.Unload(ref _scripts, capture: true) ?? "{\"actors\":[]}";
            if (editorContext is not null)
                ScriptModuleLoader.WaitForUnload(editorContext, $"{project.Name} editor scripts");

            _scripts = ScriptModuleLoader.Load(ProjectModuleId, project.ScriptAssemblyPath, state);
            if (_scripts is null && previousImage is not null)
            {
                Debug.LogError($"[Scripts] {project.Name}: the new build failed to load; running the previous build again.");
                _scripts = ScriptModuleLoader.Load(ProjectModuleId, Path.GetDirectoryName(project.ScriptAssemblyPath)!, previousImage, previousSymbols, state);
            }
            LoadEditorAssembly(project);
            Selection.Refresh();
            ScriptReloadTest.AfterReload();
            if (_scripts is not null)
                Debug.LogSuccess($"[Scripts] Reloaded {project.Name} scripts in {clock.Elapsed.TotalMilliseconds:F0} ms");
        }

        // The engine's resource system reads generated data (Library/Data/<Type>/<guid>.<ext>) by GUID from here.
        private static void AddResourceSource(ProjectInfo project)
        {
            string data = Path.Combine(project.Root, "Library", "Data");
            Directory.CreateDirectory(data);
            if (Resources.AddDirectorySource(data) == 0)
                Debug.LogWarning($"[Project] Could not add {data} as a resource source");
        }

        // Loads the project's editor scripts (<Name>.Editor.dll) into their own collectible context. The editor kits
        // resolve to this module's copies, so drawers derive from the editor's own PropertyDrawer; the runtime scripts
        // they reference load from the build folder as a reflection copy (the live game objects stay in game.project).
        private static void LoadEditorAssembly(ProjectInfo project)
        {
            if (!project.HasEditorScripts)
                return;
            if (!File.Exists(project.EditorAssemblyPath))
            {
                Debug.LogWarning($"[Project] {project.Name}: no editor script build at {project.EditorAssemblyPath}");
                return;
            }

            AssemblyLoadContext editor = TypeBridge.EditorContext;
            ScriptLoadContext context = new($"{ProjectModuleId}.editor", Path.GetDirectoryName(project.EditorAssemblyPath)!,
                name => editor.Assemblies.FirstOrDefault(a => a.GetName().Name == name.Name));
            try
            {
                Assembly assembly = context.LoadFile(project.EditorAssemblyPath);
                _editorContext = context;
                _editorAssembly = assembly;
                TypeBridge.AddContext(context);
                int drawers = DrawerRegistry.Register(assembly);
                Subsystem.Get<Tools>()?.RegisterAssembly(assembly);
                Debug.LogSuccess($"[Project] {project.Name}: loaded editor scripts ({drawers} property drawer(s)) from {project.EditorAssemblyPath}");
            }
            catch (Exception e)
            {
                Debug.LogError($"[Project] {project.Name}: failed to load editor scripts: {e.Message}");
                context.Unload();
            }
        }

        // Unregisters everything the editor scripts added and unloads their context; returns it to wait for, or null.
        [System.Runtime.CompilerServices.MethodImpl(System.Runtime.CompilerServices.MethodImplOptions.NoInlining)]
        private static WeakReference? UnloadEditorAssembly()
        {
            if (_editorContext is null || _editorAssembly is null)
                return null;
            Subsystem.Get<Tools>()?.UnregisterAssembly(_editorAssembly);
            DrawerRegistry.Unregister(_editorAssembly);
            ImUI.UnregisterFrom(_editorContext);
            TypeBridge.RemoveContext(_editorContext);
            _editorContext.Unload();
            WeakReference context = new(_editorContext);
            _editorContext = null;
            _editorAssembly = null;
            return context;
        }

        // Loads the project's script assembly as "game.project", a sibling of this editor module.
        private static void LoadScriptModule(ProjectInfo project)
        {
            Startup.Report($"Loading {project.Name} scripts", Path.GetFileName(project.ScriptAssemblyPath), Startup.Indeterminate);
            if (!File.Exists(project.ScriptAssemblyPath))
            {
                Debug.LogWarning($"[Project] {project.Name}: no script build at {project.ScriptAssemblyPath} (see the Console for build errors, or use Assets > Build Scripts).");
                return;
            }
            if (File.Exists(project.BuildFailedMarkerPath))
                Debug.LogWarning($"[Project] {project.Name}: the latest script build failed; loading the last successful build.");

            _scripts = ScriptModuleLoader.Load(ProjectModuleId, project.ScriptAssemblyPath);
            if (_scripts is not null)
                Debug.LogSuccess($"[Project] {project.Name}: loaded scripts from {project.ScriptAssemblyPath}");
            else
                Debug.LogError($"[Project] {project.Name}: failed to load {project.ScriptAssemblyPath}");
        }

        public static void OpenCSharpProject()
        {
            if (Current is null)
                return;
            ProjectFiles.Generate(Current);
            try
            {
                Process.Start(new ProcessStartInfo(Current.SlnxPath) { UseShellExecute = true });
            }
            catch (Exception e)
            {
                Debug.LogError($"[Project] Cannot open {Current.SlnxPath}: {e.Message}");
            }
        }

        // Returns why the project's script assembly must be rebuilt, or null when it is up to date with its inputs.
        public static string? ScriptsStaleReason(ProjectInfo project)
        {
            if (!File.Exists(project.ScriptAssemblyPath))
                return "not built yet";
            if (!File.Exists(project.InputsStampPath))
                return "no build record";
            if (project.HasEditorScripts && !File.Exists(project.EditorAssemblyPath))
                return "editor scripts not built yet";

            Dictionary<string, string> previous = new(StringComparer.OrdinalIgnoreCase);
            foreach (string line in File.ReadAllLines(project.InputsStampPath))
            {
                int split = line.LastIndexOf('|');
                if (split > 0)
                    previous[line[..split]] = line[(split + 1)..];
            }

            List<string> current = InputStamp(project);
            foreach (string entry in current)
            {
                int split = entry.LastIndexOf('|');
                string file = entry[..split];
                if (!previous.TryGetValue(file, out string? stamp))
                    return $"{Path.GetFileName(file)} added";
                if (stamp != entry[(split + 1)..])
                    return $"{Path.GetFileName(file)} changed";
            }
            return previous.Count != current.Count ? "script files removed" : null;
        }

        // Runs "dotnet build" on the project in the background and reports diagnostics to the Console.
        public static Task<bool> BuildScripts()
        {
            ProjectInfo? project = Current;
            if (project is null)
                return Task.FromResult(false);
            if (Interlocked.Exchange(ref _building, 1) != 0)
            {
                Debug.LogWarning("[Project] A script build is already running");
                return Task.FromResult(false);
            }

            ProjectFiles.Generate(project);
            Debug.Log($"[Project] Building {project.Name} scripts...");
            return Task.Run(() =>
            {
                try
                {
                    bool built = RunBuild(project);
                    if (built)
                    {
                        Debug.Log($"[Project] {project.Name} scripts built; reloading");
                        _mainThread.Enqueue(RequestReload);
                    }
                    return built;
                }
                finally
                {
                    Volatile.Write(ref _building, 0);
                }
            });
        }

        private static void BuildNow(ProjectInfo project, string reason)
        {
            if (Interlocked.Exchange(ref _building, 1) != 0)
                return;
            try
            {
                Debug.Log($"[Project] Building {project.Name} scripts ({reason})...");
                Startup.Report($"Building {project.Name} scripts", reason, Startup.Indeterminate);
                Stopwatch clock = Stopwatch.StartNew();
                if (RunBuild(project, pumpSplash: true, reason))
                    Debug.LogSuccess($"[Project] {project.Name} scripts built in {clock.Elapsed.TotalSeconds:F1} s");
            }
            finally
            {
                Volatile.Write(ref _building, 0);
            }
        }

        // pumpSplash: called on the main thread during startup; keeps the splash responsive and shows the build output.
        private static bool RunBuild(ProjectInfo project, bool pumpSplash = false, string? reason = null)
        {
            List<string> inputs = InputStamp(project);
            string target = project.HasEditorScripts ? project.EditorCsprojPath : project.CsprojPath;
            ProcessStartInfo info = new("dotnet", $"build \"{target}\" -nologo -v:quiet -clp:NoSummary -nodeReuse:false")
            {
                UseShellExecute = false,
                RedirectStandardOutput = true,
                RedirectStandardError = true,
                CreateNoWindow = true,
                WorkingDirectory = project.Root,
            };
            info.Environment["MSBUILDDISABLENODEREUSE"] = "1";
            info.Environment["DOTNET_CLI_TELEMETRY_OPTOUT"] = "1";

            int errors = 0;
            int warnings = 0;
            HashSet<string> reported = new();
            try
            {
                string? lastLine = null;
                using Process process = Process.Start(info)!;
                process.OutputDataReceived += (_, e) =>
                {
                    if (!string.IsNullOrWhiteSpace(e.Data))
                        Volatile.Write(ref lastLine, e.Data.Trim());
                    Report(e.Data, reported, ref errors, ref warnings);
                };
                process.ErrorDataReceived += (_, e) => Report(e.Data, reported, ref errors, ref warnings);
                process.BeginOutputReadLine();
                process.BeginErrorReadLine();
                if (!(pumpSplash ? WaitPumpingSplash(process, reason ?? Path.GetFileName(target), () => Volatile.Read(ref lastLine)) : process.WaitForExit(BuildTimeoutMilliseconds)))
                {
                    process.Kill(entireProcessTree: true);
                    MarkFailed(project, true);
                    Debug.LogError($"[Project] {project.Name} script build timed out after {BuildTimeoutMilliseconds / 1000} s");
                    return false;
                }
                process.WaitForExit();

                if (process.ExitCode == 0)
                {
                    File.WriteAllLines(project.InputsStampPath, inputs);
                    MarkFailed(project, false);
                    if (warnings > 0)
                        Debug.LogWarning($"[Project] {project.Name} scripts built with {warnings} warning(s)");
                    return true;
                }
                MarkFailed(project, true);
                Debug.LogError($"[Project] {project.Name} script build failed: {errors} error(s), {warnings} warning(s)");
                return false;
            }
            catch (Exception e)
            {
                MarkFailed(project, true);
                Debug.LogError($"[Project] Could not run dotnet build: {e.Message}");
                return false;
            }
        }

        private static bool WaitPumpingSplash(Process process, string what, Func<string?> lastLine)
        {
            Stopwatch clock = Stopwatch.StartNew();
            string? shown = null;
            while (!process.WaitForExit(50))
            {
                if (clock.ElapsedMilliseconds >= BuildTimeoutMilliseconds)
                    return false;
                string detail = lastLine() ?? $"{what} · {clock.Elapsed.TotalSeconds:F0} s";
                if (detail != shown)
                {
                    Startup.Report(null, detail, Startup.Indeterminate);
                    shown = detail;
                }
                Startup.Pump();
            }
            return true;
        }

        // One "path|lastWriteTicks|length" line per build input, sorted; compared with the record of the last build.
        private static List<string> InputStamp(ProjectInfo project)
        {
            List<string> lines = new();
            foreach (string file in project.ScriptInputs())
            {
                FileInfo info = new(file);
                if (info.Exists)
                    lines.Add($"{file}|{info.LastWriteTimeUtc.Ticks}:{info.Length}");
            }
            lines.Sort(StringComparer.OrdinalIgnoreCase);
            return lines;
        }

        private static void MarkFailed(ProjectInfo project, bool failed)
        {
            try
            {
                if (failed)
                {
                    Directory.CreateDirectory(Path.GetDirectoryName(project.BuildFailedMarkerPath)!);
                    File.WriteAllText(project.BuildFailedMarkerPath, DateTime.Now.ToString("O"), Encoding.UTF8);
                }
                else if (File.Exists(project.BuildFailedMarkerPath))
                {
                    File.Delete(project.BuildFailedMarkerPath);
                }
            }
            catch (IOException)
            {
            }
        }

        private static void Report(string? line, HashSet<string> reported, ref int errors, ref int warnings)
        {
            if (string.IsNullOrWhiteSpace(line))
                return;
            Match match = Diagnostic().Match(line);
            if (!match.Success)
                return;
            string message = $"{match.Groups["file"].Value}({match.Groups["pos"].Value}): {match.Groups["code"].Value} {match.Groups["text"].Value}";
            lock (reported)
            {
                if (!reported.Add(message))
                    return;
            }
            if (match.Groups["kind"].Value == "error")
            {
                Interlocked.Increment(ref errors);
                Debug.LogError(message);
            }
            else
            {
                Interlocked.Increment(ref warnings);
                Debug.LogWarning(message);
            }
        }

        [GeneratedRegex(@"^\s*(?<file>.+?)\((?<pos>\d+(?:,\d+)?)\):\s*(?<kind>error|warning)\s+(?<code>\w+):\s*(?<text>.*?)(?:\s+\[[^\]]+\])?$")]
        private static partial Regex Diagnostic();
    }
}
