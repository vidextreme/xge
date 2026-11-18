// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg;
using xg.imui;
using xg.math;

namespace xgEditor.CoreCLR
{
    // The content of the editor shell's bars: the toolbar (project, script build, layout) and the status bar (latest log
    // line, which focuses the Console when clicked, and the engine version).
    internal static class EditorShell
    {
        private const string LayoutPopupId = "##shell_layout";
        private const float ButtonHeight = 20.0f;
        private const float ButtonGap = 4.0f;
        private const float LayoutPopupWidth = 150.0f;

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
