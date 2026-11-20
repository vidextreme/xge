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
        private static CancellationTokenSource? _buildCancel;
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
        // Whether game.project is loaded.
        public static bool HasScripts => _scripts is not null;

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
            AssetBrowser.Current = new ProjectAssetBrowser(Assets);
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

        // Swaps in the current build of the project's scripts between frames. During play the build waits until play
        // stops (Stop reloads the scripts anyway).
        public static void RequestReload()
        {
            ProjectInfo? project = Current;
            if (project is null)
                return;
            if (PlayMode.IsActive)
            {
                Debug.Log("[Scripts] New scripts are ready; they load when play stops");
                return;
            }
            ScriptRuntime.RunBetweenFrames(() => Reload(project, null));
        }

        // The actors and component values of game.project (a ReloadState capture), or null without scripts.
        internal static string? CaptureScripts() => _scripts is null ? null : ScriptModuleLoader.Capture(_scripts);

        // Between frames, when play stops: destroys the actors spawned during play, reloads the scripts (the latest
        // build) and restores snapshot, the capture taken when play started.
        internal static void RestoreAfterPlay(string? snapshot)
        {
            ProjectInfo? project = Current;
            if (project is not null)
                Reload(project, snapshot ?? "{\"actors\":[]}");
        }

        // Runs between frames: captures the actors and component values (or takes restore, a capture to go back to),
        // unloads the editor scripts and game.project, loads the new build and restores them. A build that fails to load
        // is replaced by the previous one again.
        private static void Reload(ProjectInfo project, string? restore)
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
            if (restore is not null && _scripts is not null)
            {
                int destroyed = ScriptModuleLoader.DestroyActorsExcept(_scripts, restore);
                if (destroyed > 0)
                    Debug.Log($"[Play] Destroyed {destroyed} actor(s) spawned during play");
            }
            string captured = ScriptModuleLoader.Unload(ref _scripts, capture: true) ?? "{\"actors\":[]}";
            string state = restore ?? captured;
            if (editorContext is not null)
                ScriptModuleLoader.WaitForUnload(editorContext, $"{project.Name} editor scripts");

            _scripts = ScriptModuleLoader.Load(ProjectModuleId, project.ScriptAssemblyPath, state);
            if (_scripts is null && previousImage is not null)
            {
                Debug.LogError($"[Scripts] {project.Name}: the new build failed to load; running the previous build again.");
                _scripts = ScriptModuleLoader.Load(ProjectModuleId, Path.GetDirectoryName(project.ScriptAssemblyPath)!, previousImage, previousSymbols, state);
            }
            if (_scripts is not null)
                PlayMode.ApplyToScripts();
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
            {
                PlayMode.ApplyToScripts();
                Debug.LogSuccess($"[Project] {project.Name}: loaded scripts from {project.ScriptAssemblyPath}");
            }
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

        // Runs "dotnet build" on the project and reports diagnostics to the Console. The wait for the external process
        // happens on a thread-pool task (not a job worker, which it would hold for the whole build); the build shows in
        // the status bar's background tasks, where cancelling kills the process tree and keeps the current scripts. The
        // result is applied by a main-thread job: the task completes and a successful build reloads the scripts.
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
            CancellationTokenSource cancel = new();
            _buildCancel = cancel;
            BackgroundTask task = BackgroundTasks.Add($"Build {project.Name} scripts", Icon.Script, cancel.Cancel, "Building scripts");
            task.Start();
            task.Report(-1.0f, "dotnet build");
            return Task.Run(() =>
            {
                bool built = false;
                bool canceled = false;
                try
                {
                    built = RunBuild(project, out canceled, task: task, cancel: cancel.Token);
                }
                finally
                {
                    Volatile.Write(ref _building, 0);
                    Jobs.Schedule(() => FinishScriptBuild(project, task, cancel, built, canceled), JobQueue.Main);
                }
                return built;
            });
        }

        private static void FinishScriptBuild(ProjectInfo project, BackgroundTask task, CancellationTokenSource cancel, bool built, bool canceled)
        {
            BackgroundTasks.Complete(task);
            if (ReferenceEquals(_buildCancel, cancel))
                _buildCancel = null;
            cancel.Dispose();
            if (canceled)
            {
                Debug.Log($"[Project] {project.Name} script build canceled; the current scripts keep running");
                return;
            }
            if (!built)
                return;
            Debug.Log($"[Project] {project.Name} scripts built; reloading");
            RequestReload();
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
                if (RunBuild(project, out _, pumpSplash: true, reason))
                    Debug.LogSuccess($"[Project] {project.Name} scripts built in {clock.Elapsed.TotalSeconds:F1} s");
            }
            finally
            {
                Volatile.Write(ref _building, 0);
            }
        }

        // pumpSplash: called on the main thread during startup; keeps the splash responsive and shows the build output.
        // task gets the latest output line as its stage; cancel kills the process tree (canceled = true, nothing marked
        // as failed).
        private static bool RunBuild(ProjectInfo project, out bool canceled, bool pumpSplash = false, string? reason = null, BackgroundTask? task = null,
            CancellationToken cancel = default)
        {
            canceled = false;
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
                    {
                        Volatile.Write(ref lastLine, e.Data.Trim());
                        task?.Report(-1.0f, e.Data.Trim());
                    }
                    Report(e.Data, reported, ref errors, ref warnings);
                };
                process.ErrorDataReceived += (_, e) => Report(e.Data, reported, ref errors, ref warnings);
                process.BeginOutputReadLine();
                process.BeginErrorReadLine();
                bool exited = pumpSplash
                    ? WaitPumpingSplash(process, reason ?? Path.GetFileName(target), () => Volatile.Read(ref lastLine))
                    : WaitCancellable(process, cancel);
                if (cancel.IsCancellationRequested && !process.HasExited)
                {
                    process.Kill(entireProcessTree: true);
                    process.WaitForExit();
                    canceled = true;
                    return false;
                }
                if (!exited)
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

        // Waits for the build up to the timeout; returns early (false) when canceled.
        private static bool WaitCancellable(Process process, CancellationToken cancel)
        {
            Stopwatch clock = Stopwatch.StartNew();
            while (!process.WaitForExit(100))
            {
                if (cancel.IsCancellationRequested || clock.ElapsedMilliseconds >= BuildTimeoutMilliseconds)
                    return false;
            }
            return true;
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
