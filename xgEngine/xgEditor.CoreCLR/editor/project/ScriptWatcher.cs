// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Diagnostics;

namespace xgEditor.CoreCLR
{
    // Watches a project's scripts (Assets/**/*.cs) and manifest for changes, from any thread; the editor polls
    // TakeSettled each frame and rebuilds once the files have been quiet for a moment.
    internal sealed class ScriptWatcher : IDisposable
    {
        private readonly List<FileSystemWatcher> _watchers = new();
        private long _changedAt;

        public ScriptWatcher(ProjectInfo project)
        {
            if (Directory.Exists(project.AssetsPath))
                Add(new FileSystemWatcher(project.AssetsPath, "*.cs") { IncludeSubdirectories = true });
            Add(new FileSystemWatcher(project.Root, "*" + ProjectInfo.ManifestExtension));
        }

        // True once after the files changed and then stayed unchanged for quiet.
        public bool TakeSettled(TimeSpan quiet)
        {
            long changed = Interlocked.Read(ref _changedAt);
            if (changed == 0 || Stopwatch.GetElapsedTime(changed) < quiet)
                return false;
            return Interlocked.CompareExchange(ref _changedAt, 0, changed) == changed;
        }

        public void Dispose()
        {
            foreach (FileSystemWatcher watcher in _watchers)
                watcher.Dispose();
            _watchers.Clear();
        }

        private void Add(FileSystemWatcher watcher)
        {
            watcher.NotifyFilter = NotifyFilters.FileName | NotifyFilters.LastWrite | NotifyFilters.Size;
            watcher.Changed += (_, _) => Mark();
            watcher.Created += (_, _) => Mark();
            watcher.Deleted += (_, _) => Mark();
            watcher.Renamed += (_, _) => Mark();
            watcher.EnableRaisingEvents = true;
            _watchers.Add(watcher);
        }

        private void Mark() => Interlocked.Exchange(ref _changedAt, Stopwatch.GetTimestamp());
    }
}
