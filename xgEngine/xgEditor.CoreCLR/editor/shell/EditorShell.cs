// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg;
using xg.imui;
using xg.math;

namespace xgEditor.CoreCLR
{
    // The content of the editor shell's bars: the toolbar (project, play controls, script build, layout) and the status
    // bar (latest log line, which focuses the Console when clicked, the background task chip and the engine version).
    // Hovering the task chip opens a panel above it with one card per task (progress bar, stage, cancel).
    internal static class EditorShell
    {
        private const string LayoutPopupId = "##shell_layout";
        private const float ButtonHeight = 20.0f;
        private const float ButtonGap = 4.0f;
        private const float LayoutPopupWidth = 150.0f;
        private const float CapsuleHeight = 24.0f;
        private const float CapsulePad = 2.0f;
        private const float RoundSize = 20.0f;
        private const float PillGap = 4.0f;
        private const float StepWidth = 26.0f;
        private const float IconPillWidth = 22.0f;
        private const float TaskBarWidth = 90.0f;
        private const float TaskPanelWidth = 330.0f;
        private const float TaskHeaderHeight = 28.0f;
        private const float TaskCardHeight = 50.0f;
        private const float TaskBodyMaxHeight = 200.0f;
        private const long TaskPanelGraceMilliseconds = 250;

        private static long _taskPanelUntil;
        private static readonly bool _taskPanelPinned = Environment.GetEnvironmentVariable("XG_UI_TEST_TASKS_OPEN") == "1";

        private static string? _versionText;
        private static int _openLayoutFrames = Environment.GetEnvironmentVariable("XG_UI_TEST_SHELL_LAYOUT") == "1" ? 30 : 0;

        public static void Install()
        {
            ImUI.SetShellBar(ShellBar.Toolbar, DrawToolbar);
            ImUI.SetShellBar(ShellBar.StatusBar, DrawStatusBar);
        }

        // "/ <scene>" after the project name, with an amber dot while the active scene has unsaved changes.
        private static void DrawSceneName()
        {
            if (SceneManager.Active is not EditorScene scene)
                return;
            Gui.Label("/ " + scene.Name);
            Gui.Tooltip(scene.Path ?? "Not saved to a file yet (File > Save Scene)");
            if (scene.IsDirty)
            {
                using (var dot = Gui.Canvas("##scene_dirty", new Vector2(8.0f, 18.0f)))
                    dot.Draw.CircleFilled(new Vector2(4.0f, 9.0f), 3.5f, Style.Color(StyleColor.IconWarning));
                Gui.Tooltip("Unsaved scene changes");
            }
        }

        private static void DrawToolbar()
        {
            ProjectInfo? project = ProjectService.Current;
            Gui.Label(project?.Name ?? "No project");
            DrawSceneName();
            DrawPlayCapsule();
            RenameDialog.OnGUI();
            SaveAsDialog.OnGUI();
            ConfirmDialog.OnGUI();
            SceneDialogs.OnGUI();
            Gui.FlexibleSpace();

            float buildWidth = ToggleButton.Measure("Build Scripts", null, false);
            float layoutWidth = ToggleButton.Measure("Layout", null, false);
            bool openLayout;
            using (var canvas = Gui.Canvas("##shell_actions", new Vector2(buildWidth + ButtonGap + layoutWidth, ButtonHeight)))
            {
                Painter draw = canvas.Draw;
                bool building = ProjectService.IsBuilding;
                string? buildTip = project is null ? "Open a project first" : building ? "Building..." : "Build the project's scripts";
                if (ToggleButton.Draw(draw, "##build", Rect.FromXYWH(0.0f, 0.0f, buildWidth, ButtonHeight), Icon.Script, null, "Build Scripts", null,
                    buildTip, false, project is not null && !building, ToggleDot.None).Clicked())
                    _ = ProjectService.BuildScripts();
                openLayout = ToggleButton.Draw(draw, "##layout", Rect.FromXYWH(buildWidth + ButtonGap, 0.0f, layoutWidth, ButtonHeight), Icon.View, null, "Layout", null,
                    "Window layout", false, true, ToggleDot.None).Clicked();
            }
            Rect actions = Gui.ItemRect;
            if (_openLayoutFrames > 0 && --_openLayoutFrames == 0)
                openLayout = true;
            if (openLayout)
                Gui.OpenPopup(LayoutPopupId);

            using var popup = Gui.Popup(LayoutPopupId, new Vector2(actions.max.x - LayoutPopupWidth, actions.max.y + 3.0f), new Vector2(LayoutPopupWidth, 0.0f));
            if (!popup.IsOpen)
                return;
            float inner = LayoutPopupWidth - 8.0f;
            using var rows = Gui.Canvas("##shell_layout_rows", new Vector2(LayoutPopupWidth, ButtonHeight + 8.0f));
            if (ToggleButton.Draw(rows.Draw, "##reset_layout", Rect.FromXYWH(4.0f, 4.0f, inner, ButtonHeight), Icon.View, null, "Reset Layout", null, null,
                false, true, ToggleDot.None).Clicked())
            {
                ImUI.ResetLayout();
                Gui.ClosePopup();
            }
        }

        // The transport capsule. Edit mode: [Play] [Standalone]. Play mode expands it to the right:
        // [Pause/Resume] [+1] [Stop] timer | [Standalone], tinted green while playing and amber while paused.
        private static void DrawPlayCapsule()
        {
            PlayState state = PlayMode.State;
            bool session = state != PlayState.Edit;
            string timer = session ? PlayMode.TimerText : string.Empty;
            float timerWidth = 0.0f;
            if (session)
            {
                using (Gui.Font(FontStyle.Mono))
                    timerWidth = MathF.Ceiling(Gui.MeasureText(timer).x);
            }
            float width = CapsulePad + RoundSize + PillGap + IconPillWidth + CapsulePad;
            if (session)
                width += StepWidth + PillGap + IconPillWidth + 6.0f + timerWidth + 6.0f + 1.0f + PillGap;

            using var canvas = Gui.Canvas("##play_capsule", new Vector2(width, CapsuleHeight));
            Painter draw = canvas.Draw;
            Color32 tint = session ? PlayMode.OutlineColor(state) : Style.Color(StyleColor.Border);
            Rect body = Rect.FromXYWH(0.0f, 0.0f, width, CapsuleHeight);
            draw.RectFilled(body, Style.Color(StyleColor.SegmentBg), CapsuleHeight * 0.5f);
            draw.Rect(body, tint, CapsuleHeight * 0.5f, 1.0f);

            float top = (CapsuleHeight - RoundSize) * 0.5f;
            float x = CapsulePad;
            Rect round = Rect.FromXYWH(x, top, RoundSize, RoundSize);
            bool canToggle = true;
            Result hit = Gui.HitArea("##play_toggle", round);
            Gui.Tooltip(state switch
            {
                PlayState.Playing => "Pause",
                PlayState.Paused => "Resume",
                _ => "Play in editor",
            });
            bool hovered = canToggle && hit.Hovered();
            Vector2 center = new(round.min.x + RoundSize * 0.5f, round.min.y + RoundSize * 0.5f);
            Color32 fill;
            if (session)
            {
                fill = tint;
                fill.a = (byte)(hovered ? 90 : 60);
            }
            else
            {
                fill = Style.Color(hovered ? StyleColor.ButtonHover : StyleColor.Button);
            }
            draw.CircleFilled(center, RoundSize * 0.5f, fill);
            if (session)
                draw.Circle(center, RoundSize * 0.5f - 0.5f, tint, 1.0f);
            Color32 glyph = !canToggle ? Dim() : session ? tint : Style.Color(StyleColor.Text);
            Icon icon = state == PlayState.Playing ? Icon.Pause : Icon.Play;
            draw.Icon(icon, new Vector2(center.x + (icon == Icon.Play ? 1.0f : 0.0f), center.y), 12.0f, glyph);
            if (canToggle && hit.Clicked())
                PlayMode.TogglePlayPause();
            x += RoundSize + PillGap;

            float pillTop = (CapsuleHeight - RoundSize) * 0.5f;
            if (session)
            {
                bool paused = state == PlayState.Paused;
                Rect step = Rect.FromXYWH(x, pillTop, StepWidth, RoundSize);
                if (Pill(draw, "##play_step", step, paused ? "Step one frame" : "Step one frame (pause first)", paused))
                    PlayMode.Step();
                Vector2 plusSize = Gui.MeasureText("+1");
                draw.Text(new Vector2(MathF.Round(step.min.x + (StepWidth - plusSize.x) * 0.5f), MathF.Round(step.min.y + (RoundSize - plusSize.y) * 0.5f)),
                    paused ? Style.Color(StyleColor.Text) : Dim(), "+1");
                x += StepWidth + PillGap;

                Rect stop = Rect.FromXYWH(x, pillTop, IconPillWidth, RoundSize);
                if (Pill(draw, "##play_stop", stop, "Stop", true))
                    PlayMode.Stop();
                Vector2 stopCenter = new(stop.min.x + IconPillWidth * 0.5f, stop.min.y + RoundSize * 0.5f);
                draw.RectFilled(Rect.FromXYWH(stopCenter.x - 4.0f, stopCenter.y - 4.0f, 8.0f, 8.0f), Style.Color(StyleColor.Text), 1.0f);
                x += IconPillWidth + 6.0f;

                using (Gui.Font(FontStyle.Mono))
                {
                    float lineHeight = Gui.MeasureText("0").y;
                    draw.Text(new Vector2(x, MathF.Round((CapsuleHeight - lineHeight) * 0.5f)), Style.Color(StyleColor.TextDim), timer);
                }
                x += timerWidth + 6.0f;
                draw.Line(new Vector2(x + 0.5f, 5.0f), new Vector2(x + 0.5f, CapsuleHeight - 5.0f), Style.Color(StyleColor.Border), 1.0f);
                x += 1.0f + PillGap;
            }

            Rect standalone = Rect.FromXYWH(x, pillTop, IconPillWidth, RoundSize);
            int running = StandaloneLauncher.RunningCount;
            string standaloneTip = running > 0
                ? $"Play standalone (separate process)\n{running} running"
                : "Play standalone (separate process)";
            bool canLaunch = StandaloneLauncher.CanLaunch;
            string blocked = ProjectService.Current is null ? "Open a project to play standalone" : "Build the scripts to play standalone";
            if (Pill(draw, "##play_standalone", standalone, canLaunch ? standaloneTip : blocked, canLaunch))
                StandaloneLauncher.Launch();
            draw.Icon(Icon.PlayWindow, new Vector2(standalone.min.x + IconPillWidth * 0.5f, standalone.min.y + RoundSize * 0.5f), 14.0f,
                canLaunch ? Style.Color(StyleColor.Icon) : Dim());
        }

        // A round-ended button inside the capsule; returns whether it was clicked.
        private static bool Pill(Painter draw, string id, Rect rect, string tooltip, bool enabled)
        {
            Result hit = Gui.HitArea(id, rect);
            Gui.Tooltip(tooltip);
            bool hovered = enabled && hit.Hovered();
            draw.RectFilled(rect, Style.Color(hovered ? StyleColor.ButtonHover : StyleColor.Button), rect.Height * 0.5f);
            return enabled && hit.Clicked();
        }

        private static Color32 Dim()
        {
            Color32 dim = Style.Color(StyleColor.TextDim);
            dim.a = 120;
            return dim;
        }

        private static void DrawStatusBar()
        {
            EditorLog.Drain();
            float height = Gui.MeasureText("A").y;
            using var canvas = Gui.Canvas("##shell_status", new Vector2(0.0f, height));
            Painter draw = canvas.Draw;

            _versionText ??= "XGE " + ScriptRuntime.EngineVersion;
            Color32 dim = Style.Color(StyleColor.TextDim);
            float versionWidth = Gui.MeasureText(_versionText).x;
            draw.Text(new Vector2(canvas.Size.x - versionWidth, 0.0f), dim, _versionText);

            IReadOnlyList<LogEntry> entries = EditorLog.Entries;
            if (entries.Count == 0)
            {
                DrawTaskChip(draw, canvas.Size.x - versionWidth - 12.0f, height);
                draw.Text(new Vector2(0.0f, 0.0f), dim, "Ready");
                return;
            }

            float chipLeft = DrawTaskChip(draw, canvas.Size.x - versionWidth - 12.0f, height);
            LogEntry latest = entries[^1];
            float messageWidth = Math.Max(chipLeft - 16.0f, 0.0f);
            Rect message = Rect.FromXYWH(0.0f, 0.0f, messageWidth, height);
            ConsolePanel.LevelVisual(latest.Type, out Icon icon, out StyleColor color);
            draw.Icon(icon, new Vector2(7.0f, height * 0.5f), 12.0f, Style.Color(color));
            draw.PushClip(message);
            draw.Text(new Vector2(18.0f, 0.0f), dim, latest.FirstLine);
            draw.PopClip();
            if (Gui.HitArea("##shell_status_log", message).Clicked())
                Panel.Open<ConsolePanel>().Focus();
            Gui.Tooltip("Open the Console");
        }

        // ------------------------------------------------------------
        // Background tasks
        // ------------------------------------------------------------
        // Draws the chip ending at right (when there are tasks) and the hover panel; returns the chip's left edge.
        private static float DrawTaskChip(Painter draw, float right, float height)
        {
            IReadOnlyList<BackgroundTask> tasks = BackgroundTasks.Tasks;
            if (tasks.Count == 0)
            {
                _taskPanelUntil = 0;
                return right;
            }
            int running = tasks.Count(t => t.State != BackgroundTaskState.Queued);
            string text = BackgroundTasks.BatchTotal == 1
                ? BackgroundTasks.Verb()
                : $"{BackgroundTasks.Verb()} {Math.Min(BackgroundTasks.BatchDone + running, BackgroundTasks.BatchTotal)} of {BackgroundTasks.BatchTotal}";
            float textWidth = Gui.MeasureText(text).x;
            float width = 8.0f + 12.0f + 6.0f + textWidth + 8.0f + TaskBarWidth + 8.0f;
            Rect chip = Rect.FromXYWH(right - width, -1.0f, width, height + 2.0f);
            Result hit = Gui.HitArea("##shell_tasks", chip);
            draw.RectFilled(chip, Style.Color(hit.Hovered() ? StyleColor.ButtonHover : StyleColor.HeaderBg), 4.0f);
            draw.Rect(chip, Style.Color(StyleColor.Border), 4.0f);
            float cy = chip.min.y + chip.Height * 0.5f;
            DrawSpinner(draw, new Vector2(chip.min.x + 14.0f, cy), 5.0f);
            draw.Text(new Vector2(chip.min.x + 26.0f, 0.0f), Style.Color(StyleColor.Text), text);
            Rect bar = Rect.FromXYWH(chip.max.x - 8.0f - TaskBarWidth, cy - 2.0f, TaskBarWidth, 4.0f);
            bool unknown = tasks.All(t => t.State != BackgroundTaskState.Queued && t.Progress < 0.0f);
            DrawBar(draw, bar, unknown ? -1.0f : BackgroundTasks.OverallProgress());

            long now = Environment.TickCount64;
            if (hit.Hovered())
                _taskPanelUntil = now + TaskPanelGraceMilliseconds;
            if (_taskPanelPinned || now < _taskPanelUntil)
                DrawTaskPanel(chip, tasks, running);
            return chip.min.x;
        }

        private static void DrawTaskPanel(Rect chip, IReadOnlyList<BackgroundTask> tasks, int running)
        {
            float bodyHeight = Math.Min(tasks.Count * TaskCardHeight, TaskBodyMaxHeight);
            float panelHeight = TaskHeaderHeight + bodyHeight + 2.0f;
            Vector2 position = new(chip.max.x - TaskPanelWidth, chip.min.y - panelHeight - 6.0f);
            using var overlay = Gui.Overlay("shell_tasks_panel", position, new Vector2(TaskPanelWidth, panelHeight));
            if (!overlay.IsOpen)
                return;
            bool hovered = false;
            using (var header = Gui.Canvas("##tasks_header", new Vector2(TaskPanelWidth, TaskHeaderHeight)))
            {
                Painter draw = header.Draw;
                Rect area = Rect.FromXYWH(0.0f, 0.0f, TaskPanelWidth, TaskHeaderHeight);
                hovered |= Gui.HitArea("##tasks_header_hit", area).Hovered();
                draw.RectFilled(area, Style.Color(StyleColor.PanelHeaderBg));
                DrawSpinner(draw, new Vector2(16.0f, TaskHeaderHeight * 0.5f), 5.0f);
                float textY = (TaskHeaderHeight - Gui.MeasureText("A").y) * 0.5f;
                const string title = "Background tasks";
                float x = 30.0f;
                using (Gui.Font(FontStyle.Bold))
                {
                    draw.Text(new Vector2(x, textY), Style.Color(StyleColor.Text), title);
                    x += Gui.MeasureText(title).x + 6.0f;
                }
                draw.Text(new Vector2(x, textY), Style.Color(StyleColor.TextDim), $"· {running} running, {tasks.Count - running} queued");
            }

            using (var body = Gui.ScrollArea("##tasks_body", new Vector2(TaskPanelWidth, bodyHeight)))
            {
                if (body.IsOpen)
                {
                    float width = Math.Max(Gui.AvailableSize.x, 1.0f);
                    using var canvas = Gui.Canvas("##tasks_cards", new Vector2(width, tasks.Count * TaskCardHeight));
                    Painter draw = canvas.Draw;
                    float y = 0.0f;
                    foreach (BackgroundTask task in tasks.OrderBy(t => t.State == BackgroundTaskState.Queued).ThenBy(t => t.Id))
                    {
                        hovered |= DrawTaskCard(draw, task, Rect.FromXYWH(0.0f, y, width, TaskCardHeight));
                        y += TaskCardHeight;
                    }
                }
            }
            if (hovered)
                _taskPanelUntil = Environment.TickCount64 + TaskPanelGraceMilliseconds;
        }

        private static bool DrawTaskCard(Painter draw, BackgroundTask task, Rect card)
        {
            bool hovered = Gui.HitArea($"##task{task.Id}", card).Hovered();
            draw.Line(new Vector2(card.min.x, card.max.y - 0.5f), new Vector2(card.max.x, card.max.y - 0.5f), Style.Color(StyleColor.Border));
            BackgroundTaskState state = task.State;
            float progress = task.Progress;
            Color32 text = Style.Color(state == BackgroundTaskState.Queued ? StyleColor.TextDim : StyleColor.Text);
            Color32 dim = Style.Color(StyleColor.TextDim);
            float lineHeight = Gui.MeasureText("A").y;
            draw.Icon(task.Icon, new Vector2(card.min.x + 16.0f, card.min.y + 6.0f + lineHeight * 0.5f), 13.0f, Style.Color(StyleColor.Icon));

            float right = card.max.x - 8.0f;
            if (task.CanCancel && state != BackgroundTaskState.Canceling)
            {
                Rect cancel = Rect.FromXYWH(right - 16.0f, card.min.y + 5.0f, 16.0f, 16.0f);
                Result hit = Gui.HitArea($"##task_cancel{task.Id}", cancel);
                hovered |= hit.Hovered();
                Gui.Tooltip("Cancel");
                if (hit.Hovered())
                    draw.RectFilled(cancel, Style.Color(StyleColor.ButtonHover), 3.0f);
                draw.Icon(Icon.WinClose, new Vector2(cancel.min.x + 8.0f, cancel.min.y + 8.0f), 10.0f, dim);
                if (hit.Clicked())
                    task.Cancel();
                right = cancel.min.x - 6.0f;
            }
            TimeSpan elapsed = task.Elapsed;
            string amount = state == BackgroundTaskState.Queued ? "Queued"
                : progress < 0.0f ? $"{(int)elapsed.TotalMinutes}:{elapsed.Seconds:00}"
                : $"{Math.Clamp(progress, 0.0f, 1.0f) * 100.0f:0}%";
            float amountWidth = Gui.MeasureText(amount).x;
            draw.Text(new Vector2(right - amountWidth, card.min.y + 6.0f), dim, amount);
            draw.PushClip(Rect.FromXYWH(card.min.x + 30.0f, card.min.y, Math.Max(right - amountWidth - 36.0f - card.min.x, 1.0f), card.Height));
            draw.Text(new Vector2(card.min.x + 30.0f, card.min.y + 6.0f), text, task.Title);
            draw.Text(new Vector2(card.min.x + 30.0f, card.min.y + 8.0f + lineHeight), dim, task.Stage);
            draw.PopClip();
            Rect bar = Rect.FromXYWH(card.min.x + 30.0f, card.max.y - 9.0f, card.Width - 38.0f, 4.0f);
            DrawBar(draw, bar, state == BackgroundTaskState.Queued ? 0.0f : progress);
            return hovered;
        }

        // A 4 px bar: filled to progress (0..1), or a moving segment when progress is below 0.
        private static void DrawBar(Painter draw, Rect bar, float progress)
        {
            draw.RectFilled(bar, Style.Color(StyleColor.SegmentBg), 2.0f);
            Color32 accent = Style.Color(StyleColor.Accent);
            if (progress < 0.0f)
            {
                float t = Environment.TickCount64 % 1200 / 1200.0f;
                float segment = bar.Width * 0.3f;
                float x = bar.min.x - segment + (bar.Width + segment) * t;
                float left = Math.Max(x, bar.min.x);
                float rightEdge = Math.Min(x + segment, bar.max.x);
                if (rightEdge > left)
                    draw.RectFilled(new Rect(new Vector2(left, bar.min.y), new Vector2(rightEdge, bar.max.y)), accent, 2.0f);
                return;
            }
            float fill = bar.Width * Math.Clamp(progress, 0.0f, 1.0f);
            if (fill >= 1.0f)
                draw.RectFilled(Rect.FromXYWH(bar.min.x, bar.min.y, fill, bar.Height), accent, 2.0f);
        }

        // A rotating three-quarter arc.
        private static void DrawSpinner(Painter draw, Vector2 center, float radius)
        {
            const int Segments = 12;
            float start = Environment.TickCount64 % 1000 / 1000.0f * MathF.Tau;
            Span<Vector2> points = stackalloc Vector2[Segments + 1];
            for (int i = 0; i <= Segments; i++)
            {
                float angle = start + i * (MathF.Tau * 0.75f / Segments);
                points[i] = new Vector2(center.x + MathF.Cos(angle) * radius, center.y + MathF.Sin(angle) * radius);
            }
            draw.Polyline(points, Style.Color(StyleColor.Accent), false, 1.6f);
        }
    }
}
