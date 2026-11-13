// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

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
    // loaded in the same session.
    public static partial class ProjectService
    {
        private const int BuildTimeoutMilliseconds = 120_000;

        // Script module id of the open project's scripts.
        public const string ProjectModuleId = "game.project";

        private static int _building;

        public static ProjectInfo? Current { get; private set; }
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

            ProjectFiles.Generate(Current);
            Menu.Add("Assets/Open C# Project", OpenCSharpProject);
            Menu.Add("Assets/Build Scripts", () => BuildScripts());
            Debug.Log($"[Project] Opened {Current.Name} ({Current.Root})");

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
        }

        // Loads the project's editor scripts (<Name>.Editor.dll) into this module's load context, so its drawers derive
        // from the editor's own PropertyDrawer. Its reference to the runtime scripts resolves to a reflection copy from
        // the build folder; the live game objects stay in game.project. Loaded from bytes, so rebuilds can replace it.
        private static void LoadEditorAssembly(ProjectInfo project)
        {
            if (!project.HasEditorScripts)
                return;
            if (!File.Exists(project.EditorAssemblyPath))
            {
                Debug.LogWarning($"[Project] {project.Name}: no editor script build at {project.EditorAssemblyPath}");
                return;
            }

            AssemblyLoadContext context = TypeBridge.EditorContext;
            string folder = Path.GetDirectoryName(project.EditorAssemblyPath)!;
            context.Resolving += (loader, name) =>
            {
                string candidate = Path.Combine(folder, name.Name + ".dll");
                return File.Exists(candidate) ? LoadFromBytes(loader, candidate) : null;
            };
            try
            {
                Assembly editor = LoadFromBytes(context, project.EditorAssemblyPath);
                int drawers = DrawerRegistry.Register(editor);
                Debug.LogSuccess($"[Project] {project.Name}: loaded editor scripts ({drawers} property drawer(s)) from {project.EditorAssemblyPath}");
            }
            catch (Exception e)
            {
                Debug.LogError($"[Project] {project.Name}: failed to load editor scripts: {e.Message}");
            }
        }

        private static Assembly LoadFromBytes(AssemblyLoadContext context, string path)
        {
            string pdb = Path.ChangeExtension(path, ".pdb");
            using MemoryStream assembly = new(File.ReadAllBytes(path));
            if (!File.Exists(pdb))
                return context.LoadFromStream(assembly);
            using MemoryStream symbols = new(File.ReadAllBytes(pdb));
            return context.LoadFromStream(assembly, symbols);
        }

        // Loads the project's script assembly as "game.project", a sibling of this editor module.
        private static void LoadScriptModule(ProjectInfo project)
        {
            if (!File.Exists(project.ScriptAssemblyPath))
            {
                Debug.LogWarning($"[Project] {project.Name}: no script build at {project.ScriptAssemblyPath} (see the Console for build errors, or use Assets > Build Scripts).");
                return;
            }
            if (File.Exists(project.BuildFailedMarkerPath))
                Debug.LogWarning($"[Project] {project.Name}: the latest script build failed; loading the last successful build.");

            if (ScriptRuntime.AddScriptModule(ProjectModuleId, project.ScriptAssemblyPath))
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
                        Debug.LogSuccess($"[Project] {project.Name} scripts built. Restart the editor to load them.");
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
                Stopwatch clock = Stopwatch.StartNew();
                if (RunBuild(project))
                    Debug.LogSuccess($"[Project] {project.Name} scripts built in {clock.Elapsed.TotalSeconds:F1} s");
            }
            finally
            {
                Volatile.Write(ref _building, 0);
            }
        }

        private static bool RunBuild(ProjectInfo project)
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
                using Process process = Process.Start(info)!;
                process.OutputDataReceived += (_, e) => Report(e.Data, reported, ref errors, ref warnings);
                process.ErrorDataReceived += (_, e) => Report(e.Data, reported, ref errors, ref warnings);
                process.BeginOutputReadLine();
                process.BeginErrorReadLine();
                if (!process.WaitForExit(BuildTimeoutMilliseconds))
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
