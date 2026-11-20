// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg;
using xg.imui;

namespace xgEditor.CoreCLR
{
    public enum BackgroundTaskState
    {
        Queued,
        Running,
        Canceling,
    }

    // One unit of background work shown in the status bar's task panel (an asset build). Add and Complete run on the
    // main thread; Report may be called from the worker doing the work.
    public sealed class BackgroundTask
    {
        private readonly object _lock = new();
        private readonly Action? _cancel;
        private float _progress;
        private string _stage = "Queued";
        private BackgroundTaskState _state = BackgroundTaskState.Queued;

        private long _started;

        internal BackgroundTask(long id, string title, Icon icon, string verb, Action? cancel)
        {
            Id = id;
            Title = title;
            Icon = icon;
            Verb = verb;
            _cancel = cancel;
        }

        public long Id { get; }
        public string Title { get; }
        public Icon Icon { get; }
        // What the status bar chip says while only this kind of task runs ("Importing", "Building scripts").
        public string Verb { get; }

        // Time since Start (zero while queued).
        public TimeSpan Elapsed
        {
            get
            {
                long started = Interlocked.Read(ref _started);
                return started == 0 ? TimeSpan.Zero : System.Diagnostics.Stopwatch.GetElapsedTime(started);
            }
        }
        public bool CanCancel => _cancel is not null;

        public BackgroundTaskState State
        {
            get { lock (_lock) return _state; }
        }

        // 0..1, or below 0 while the amount of work isn't known.
        public float Progress
        {
            get { lock (_lock) return _progress; }
        }

        public string Stage
        {
            get { lock (_lock) return _stage; }
        }

        public void Start()
        {
            lock (_lock)
            {
                if (_state == BackgroundTaskState.Queued)
                    _state = BackgroundTaskState.Running;
                _stage = "Starting";
            }
            Interlocked.Exchange(ref _started, System.Diagnostics.Stopwatch.GetTimestamp());
        }

        public void Report(float progress, string stage)
        {
            lock (_lock)
            {
                _progress = progress;
                _stage = stage;
            }
        }

        // Asks the owner to stop the work (main thread). The owner completes the task when it has stopped.
        public void Cancel()
        {
            if (_cancel is null)
                return;
            lock (_lock)
            {
                if (_state == BackgroundTaskState.Canceling)
                    return;
                _state = BackgroundTaskState.Canceling;
                _stage = "Canceling";
            }
            _cancel();
        }
    }

    // The editor's background tasks, drawn by EditorShell's status bar chip and its hover panel. BatchTotal and
    // BatchDone count the tasks since the list was last empty ("Importing 4 of 18").
    public static class BackgroundTasks
    {
        private static readonly List<BackgroundTask> _tasks = new();
        private static long _nextId;

        public static IReadOnlyList<BackgroundTask> Tasks => _tasks;
        public static int BatchTotal { get; private set; }
        public static int BatchDone { get; private set; }

        public static BackgroundTask Add(string title, Icon icon, Action? cancel = null, string verb = "Importing")
        {
            if (_tasks.Count == 0)
            {
                BatchTotal = 0;
                BatchDone = 0;
            }
            BackgroundTask task = new(++_nextId, title, icon, verb, cancel);
            _tasks.Add(task);
            BatchTotal++;
            return task;
        }

        public static void Complete(BackgroundTask task)
        {
            if (_tasks.Remove(task))
                BatchDone++;
        }

        public static BackgroundTask? Find(string title) =>
            _tasks.FirstOrDefault(t => t.Title.Contains(title, StringComparison.OrdinalIgnoreCase));

        // The verb shared by every task, or "Working" for a mix.
        public static string Verb()
        {
            string? verb = null;
            foreach (BackgroundTask task in _tasks)
            {
                if (verb is null)
                    verb = task.Verb;
                else if (verb != task.Verb)
                    return "Working";
            }
            return verb ?? "Working";
        }

        // Finished work plus the progress of running tasks, over the batch.
        public static float OverallProgress()
        {
            if (BatchTotal == 0)
                return 0.0f;
            float running = 0.0f;
            foreach (BackgroundTask task in _tasks)
                if (task.State != BackgroundTaskState.Queued)
                    running += Math.Clamp(task.Progress, 0.0f, 1.0f);
            return Math.Clamp((BatchDone + running) / BatchTotal, 0.0f, 1.0f);
        }
    }
}
