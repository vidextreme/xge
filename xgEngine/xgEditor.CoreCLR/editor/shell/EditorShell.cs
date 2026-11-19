// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg;
using xg.imui;
using xg.math;

namespace xgEditor.CoreCLR
{
    // The content of the editor shell's bars: the toolbar (project, play controls, script build, layout) and the status
    // bar (latest log line, which focuses the Console when clicked, and the engine version).
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

        private static string? _versionText;
        private static int _openLayoutFrames = Environment.GetEnvironmentVariable("XG_UI_TEST_SHELL_LAYOUT") == "1" ? 30 : 0;

        public static void Install()
        {
            ImUI.SetShellBar(ShellBar.Toolbar, DrawToolbar);
            ImUI.SetShellBar(ShellBar.StatusBar, DrawStatusBar);
        }

        private static void DrawToolbar()
        {
            ProjectInfo? project = ProjectService.Current;
            Gui.Label(project?.Name ?? "No project");
            DrawPlayCapsule();
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
                draw.Text(new Vector2(0.0f, 0.0f), dim, "Ready");
                return;
            }

            LogEntry latest = entries[^1];
            float messageWidth = Math.Max(canvas.Size.x - versionWidth - 16.0f, 0.0f);
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
    }
}
