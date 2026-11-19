// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Diagnostics;
using System.Text.Json;
using System.Text.Json.Nodes;
using xg;
using xg.editor;
using Debug = xg.Debug;

namespace xgEditor.CoreCLR
{
    // Play Standalone: runs the project's current script build in a separate xge process without the editor. Each
    // launch copies the build into its own folder under Library/Play/Standalone (so the running game never locks the
    // editor's build output) and writes a config there that loads only game.kernel and the scripts.
    public static class StandaloneLauncher
    {
        private const string FolderPrefix = "run-";

        private static readonly List<Process> _running = new();

        // Standalone processes started from this editor that are still running.
        public static int RunningCount
        {
            get
            {
                _running.RemoveAll(p => HasExited(p));
                return _running.Count;
            }
        }

        public static bool CanLaunch => ProjectService.Current is { } project && File.Exists(project.ScriptAssemblyPath);

        public static Process? Launch()
        {
            ProjectInfo? project = ProjectService.Current;
            if (project is null || !File.Exists(project.ScriptAssemblyPath))
            {
                Debug.LogWarning("[Play] No script build to run standalone (build the scripts first)");
                return null;
            }
            string root = Path.Combine(project.LibraryPath, "Play", "Standalone");
            RemoveOldRuns(root);
            string folder = Path.Combine(root, FolderPrefix + DateTime.Now.ToString("yyyyMMdd-HHmmss-fff"));
            try
            {
                Directory.CreateDirectory(folder);
                string build = Path.GetDirectoryName(project.ScriptAssemblyPath)!;
                foreach (string file in Directory.EnumerateFiles(build))
                {
                    string name = Path.GetFileName(file);
                    if (name.StartsWith(project.EditorAssemblyName, StringComparison.OrdinalIgnoreCase) || name is "inputs.txt" or "build-failed")
                        continue;
                    File.Copy(file, Path.Combine(folder, name), true);
                }
                string config = Path.Combine(folder, "standalone.config");
                JSONFile.Write(config, Config(project, Path.Combine(folder, Path.GetFileName(project.ScriptAssemblyPath))));

                string xge = Environment.ProcessPath ?? Path.Combine(AppContext.BaseDirectory, "xge.exe");
                ProcessStartInfo info = new(xge)
                {
                    UseShellExecute = false,
                    CreateNoWindow = true,
                    WorkingDirectory = Path.GetDirectoryName(xge)!,
                };
                info.ArgumentList.Add("--config");
                info.ArgumentList.Add(config);
                Process? process = Process.Start(info);
                if (process is null)
                {
                    Debug.LogError("[Play] Could not start the standalone player");
                    return null;
                }
                _running.Add(process);
                Debug.Log($"[Play] Standalone started (pid {process.Id}) from {folder}");
                return process;
            }
            catch (Exception e) when (e is IOException or UnauthorizedAccessException or System.ComponentModel.Win32Exception)
            {
                Debug.LogError($"[Play] Could not start the standalone player: {e.Message}");
                return null;
            }
        }

        private static JsonObject Config(ProjectInfo project, string scripts)
        {
            JsonObject engine = new()
            {
                ["RendererModule"] = EditorRendererModule(),
                ["EditorUI"] = false,
                ["EditorMultiViewport"] = false,
                ["WindowTitle"] = $"{project.Name} (standalone)",
                ["WindowWidth"] = 1280,
                ["WindowHeight"] = 720,
                ["Splash"] = false,
            };
            JsonObject modules = new()
            {
                ["game.kernel"] = new JsonObject
                {
                    ["path"] = "xgGameKernel.dll",
                    ["group"] = "group.main",
                    ["children"] = new JsonObject { [ProjectService.ProjectModuleId] = scripts },
                },
            };
            return new JsonObject { ["EngineConfig"] = engine, ["ScriptModules"] = modules };
        }

        // The renderer of the default gameconfig.json next to xge.exe (DX12 without one).
        private static string EditorRendererModule()
        {
            try
            {
                string path = Path.Combine(Path.GetDirectoryName(Environment.ProcessPath ?? string.Empty) ?? string.Empty, "gameconfig.json");
                if (File.Exists(path) && JsonNode.Parse(File.ReadAllText(path))?["EngineConfig"]?["RendererModule"] is JsonNode renderer)
                    return renderer.GetValue<string>();
            }
            catch (Exception e) when (e is IOException or JsonException or InvalidOperationException)
            {
            }
            return "xgRendererDX12";
        }

        private static void RemoveOldRuns(string root)
        {
            if (!Directory.Exists(root))
                return;
            foreach (string folder in Directory.EnumerateDirectories(root, FolderPrefix + "*"))
            {
                try
                {
                    Directory.Delete(folder, true);
                }
                catch (Exception e) when (e is IOException or UnauthorizedAccessException)
                {
                }
            }
        }

        private static bool HasExited(Process process)
        {
            try
            {
                return process.HasExited;
            }
            catch (InvalidOperationException)
            {
                return true;
            }
        }
    }
}
