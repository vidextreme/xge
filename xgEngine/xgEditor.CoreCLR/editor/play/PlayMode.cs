// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg;
using xg.imui;
using xg.math;
using Debug = xg.Debug;

namespace xgEditor.CoreCLR
{
    public enum PlayState
    {
        Edit,
        Playing,
        Paused,
    }

    // Play in Editor. In edit mode the project's scripts (game.project) are loaded but paused: their actors exist and can
    // be inspected, but no game or component update runs. Play snapshots the actors and their component values and
    // resumes the module; Pause/Step/Resume drive it through the engine's module pause; Stop pauses it again, destroys
    // the actors spawned during play, reloads the scripts and restores the snapshot. Script builds made during play load
    // when play stops. Transitions run between frames.
    public static class PlayMode
    {
        private const string TestHookVariable = "XG_UI_TEST_PLAY";
        private const int TestStepFrames = 60;

        private static string? _snapshot;
        private static string? _editorSnapshot;
        private static Transform.Snapshot? _transforms;
        private static ComponentData.Snapshot? _data;
        private static double _time;
        private static long _frames;
        private static PlayState _outline = PlayState.Edit;
        private static readonly Queue<string> _testActions = new();
        private static int _testFrames;
        private static bool _testStandalone = Environment.GetEnvironmentVariable("XG_UI_TEST_PLAY_STANDALONE") == "1";

        public static PlayState State { get; private set; } = PlayState.Edit;
        public static bool IsActive => State != PlayState.Edit;
        // Game time and game frames since Play (steps count one frame of 1/60 s).
        public static double Time => _time;
        public static long Frames => _frames;

        public static bool CanPlay => true;

        // Edit mode runs no component updates in any module.
        public static void Initialize()
        {
            ScriptRuntime.ActorUpdatesPaused = true;
        }

        static PlayMode()
        {
            string? actions = Environment.GetEnvironmentVariable(TestHookVariable);
            if (!string.IsNullOrEmpty(actions))
                foreach (string action in actions.Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries))
                    _testActions.Enqueue(action.ToLowerInvariant());
        }

        // "02:05.3 · f7512"
        public static string TimerText
        {
            get
            {
                int minutes = (int)(_time / 60.0);
                double seconds = _time - minutes * 60.0;
                return $"{minutes:00}:{seconds:00.0} · f{_frames}";
            }
        }

        // The round button: Play in edit mode, Pause while playing, Resume while paused.
        public static void TogglePlayPause()
        {
            switch (State)
            {
                case PlayState.Edit: Play(); break;
                case PlayState.Playing: Pause(); break;
                case PlayState.Paused: Resume(); break;
            }
        }

        public static void Play()
        {
            if (State != PlayState.Edit)
                return;
            ScriptRuntime.RunBetweenFrames(() =>
            {
                if (State != PlayState.Edit)
                    return;
                _editorSnapshot = ReloadState.Capture();
                _transforms = Transform.Snapshot.Capture();
                _data = ComponentData.Snapshot.Capture();
                _snapshot = ProjectService.HasScripts ? ProjectService.CaptureScripts() : null;
                _time = 0.0;
                _frames = 0;
                SetRunning(true);
                SetState(PlayState.Playing);
            });
        }

        public static void Pause()
        {
            if (State != PlayState.Playing)
                return;
            SetRunning(false);
            SetState(PlayState.Paused);
        }

        public static void Resume()
        {
            if (State != PlayState.Paused)
                return;
            SetRunning(true);
            SetState(PlayState.Playing);
        }

        // One game frame while paused.
        public static void Step()
        {
            if (State != PlayState.Paused)
                return;
            ScriptRuntime.StepActorUpdates();
            if (ProjectService.HasScripts)
                ScriptRuntime.StepModule(ProjectService.ProjectModuleId);
            _frames++;
            _time += 1.0 / 60.0;
        }

        // Everything changed during play is temporary: the project's scripts are reloaded with the snapshot taken at
        // Play, and the editor module's own actors are put back in place.
        public static void Stop()
        {
            if (State == PlayState.Edit)
                return;
            SetRunning(false);
            ScriptRuntime.RunBetweenFrames(() =>
            {
                if (State == PlayState.Edit)
                    return;
                string? snapshot = _snapshot;
                string? editorSnapshot = _editorSnapshot;
                Transform.Snapshot? transforms = _transforms;
                ComponentData.Snapshot? data = _data;
                _data = null;
                _snapshot = null;
                _editorSnapshot = null;
                _transforms = null;
                SetState(PlayState.Edit);
                if (editorSnapshot is not null)
                {
                    int destroyed = ReloadState.DestroyActorsExcept(editorSnapshot);
                    Debug.Log($"[Play] Restored editor actors: {ReloadState.Apply(editorSnapshot)}, {destroyed} spawned destroyed");
                }
                if (ProjectService.HasScripts)
                    ProjectService.RestoreAfterPlay(snapshot);
                if (transforms is not null)
                {
                    (int restored, int readded, int removed) = transforms.Restore();
                    Debug.Log($"[Play] Restored transforms: {restored} restored, {readded} re-added, {removed} removed");
                }
                if (data is not null)
                {
                    (int restored, int removed) = data.Restore();
                    Debug.Log($"[Play] Restored component data: {restored} element(s), {removed} removed");
                }
                Debug.Log($"[Play] Stopped after {_frames} frame(s), {_time:F2} s");
            });
        }

        private static void SetRunning(bool running)
        {
            ScriptRuntime.ActorUpdatesPaused = !running;
            if (ProjectService.HasScripts)
                ScriptRuntime.SetModulePaused(ProjectService.ProjectModuleId, !running);
        }

        // Called after the project's scripts load (startup, hot reload, Stop): they stay paused outside play.
        public static void ApplyToScripts()
        {
            ScriptRuntime.SetModulePaused(ProjectService.ProjectModuleId, State != PlayState.Playing);
        }

        // Per editor frame (main thread).
        public static void Update(float dt)
        {
            if (State == PlayState.Playing)
            {
                _frames++;
                _time += dt;
            }
            if (_outline != State)
            {
                _outline = State;
                ImUI.SetDockOutline(OutlineColor(State));
            }
            UpdateTestHooks();
        }

        // Green while playing, amber while paused, none in edit mode.
        public static Color32 OutlineColor(PlayState state) => state switch
        {
            PlayState.Playing => Style.Color(StyleColor.AxisY),
            PlayState.Paused => Style.Color(StyleColor.IconWarning),
            _ => default,
        };

        private static void SetState(PlayState state)
        {
            if (State == state)
                return;
            State = state;
            Debug.Log($"[Play] {state} (frame {_frames}, {_time:F2} s)");
        }

        private static void UpdateTestHooks()
        {
            if (_testActions.Count == 0 && !_testStandalone)
                return;
            if (++_testFrames < TestStepFrames)
                return;
            _testFrames = 0;
            if (_testStandalone)
            {
                _testStandalone = false;
                StandaloneLauncher.Launch();
                return;
            }
            switch (_testActions.Dequeue())
            {
                case "play": Play(); break;
                case "pause": Pause(); break;
                case "resume": Resume(); break;
                case "step": Step(); Debug.Log($"[Play] Step requested (frame {_frames})"); break;
                case "stop": Stop(); break;
                case "frames": Debug.Log($"[Play] {State}: frame {_frames}, {_time:F2} s"); break;
                case "state":
                    Debug.Log($"[Play] {State} editor state: {ReloadState.Capture()}");
                    if (ProjectService.HasScripts)
                        Debug.Log($"[Play] {State} scripts state: {ProjectService.CaptureScripts()}");
                    break;
            }
        }
    }
}
