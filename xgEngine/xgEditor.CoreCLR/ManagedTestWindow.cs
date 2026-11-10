// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg;
using xg.imui;
using xg.math;

namespace xgEditor.CoreCLR
{
    // Test hook: exercises the managed xg.imui API (widgets + immediate drawing). Mirrors the native "Native Test".
    // XG_UI_TEST_FOCUS=managed brings it to the front after startup; XG_UI_TEST_THROW=1 throws from OnGUI every frame.
    internal sealed class ManagedTestWindow : Panel
    {
        private float _speed = 2.5f;
        private int _clicks;
        private bool _reportedDraw;
        private readonly bool _throw = Environment.GetEnvironmentVariable("XG_UI_TEST_THROW") == "1";

        public override string Title => "Managed Test";
        public override string? MenuPath => "Tests/Managed Test";
        public override DockSlot Slot => DockSlot.Right;
        public override Icon PanelIcon => Icon.Script;

        protected override void OnEnable() => Debug.Log("[Debug] Managed Test: window enabled");
        protected override void OnDisable() => Debug.Log("[Debug] Managed Test: window disabled");

        protected override void OnGUI()
        {
            Gui.Label("Managed xg.imui panel");
            if (Gui.Button("Click me"))
                Debug.Log($"[Debug] Managed Test: button clicked ({++_clicks})");
            if (Gui.Field("Speed", ref _speed))
                Debug.Log($"[Debug] Managed Test: speed changed to {_speed:F2}");

            using (CanvasScope canvas = Gui.Canvas("##managed_canvas", new Vector2(0.0f, 160.0f)))
            {
                Painter draw = canvas.Draw;
                Vector2 size = canvas.Size;
                draw.RectFilled(Rect.FromXYWH(0.0f, 0.0f, size.x, size.y), Style.Color(StyleColor.ViewportBg), 4.0f);
                Color32 gridColor = new(255, 255, 255, 24);
                for (float x = 16.0f; x < size.x; x += 16.0f)
                    draw.Line(new Vector2(x, 0.0f), new Vector2(x, size.y), gridColor);
                for (float y = 16.0f; y < size.y; y += 16.0f)
                    draw.Line(new Vector2(0.0f, y), new Vector2(size.x, y), gridColor);
                Vector2 center = new(size.x * 0.5f, size.y * 0.5f);
                draw.Circle(center, 28.0f + _speed * 4.0f, Style.Color(StyleColor.Accent), 2.0f);
                draw.CircleFilled(center, 6.0f, Style.Color(StyleColor.ActiveObject));
                draw.Icon(Icon.Script, new Vector2(20.0f, 20.0f), 16.0f, Style.Color(StyleColor.Icon));
                draw.Text(new Vector2(34.0f, 12.0f), Style.Color(StyleColor.Text), "Painter (managed)"u8);

                if (!_reportedDraw)
                {
                    _reportedDraw = true;
                    Debug.Log($"[Debug] Managed Test: drew canvas {size.x:F0}x{size.y:F0}");
                }
            }

            if (_throw)
                throw new InvalidOperationException("Managed Test: XG_UI_TEST_THROW");
        }
    }

    // Test hook: a floating (undocked) managed panel with a live animated canvas. Mirrors "Native Floating".
    internal sealed class ManagedFloatingWindow : Panel
    {
        private readonly System.Diagnostics.Stopwatch _clock = System.Diagnostics.Stopwatch.StartNew();
        private int _clicks;

        public override string Title => "Managed Floating";
        public override string? MenuPath => "Tests/Managed Floating";
        public override DockSlot Slot => DockSlot.Floating;
        public override Icon PanelIcon => Icon.Script;
        public override Rect? InitialRect =>
            Environment.GetEnvironmentVariable("XG_UI_TEST_FLOAT_OUTSIDE") == "1"
                ? Rect.FromXYWH(1100.0f, 320.0f, 300.0f, 230.0f)
                : Rect.FromXYWH(570.0f, 140.0f, 300.0f, 230.0f);

        protected override void OnGUI()
        {
            Gui.Label("Floating managed panel");
            if (Gui.Button("Count"))
                Debug.Log($"[Debug] Managed Floating: count {++_clicks}");

            using CanvasScope canvas = Gui.Canvas("##managed_floating_canvas");
            Painter draw = canvas.Draw;
            Vector2 size = canvas.Size;
            Vector2 center = new(size.x * 0.5f, size.y * 0.5f);
            float radius = MathF.Min(size.x, size.y) * 0.4f;
            float angle = (float)_clock.Elapsed.TotalSeconds * 1.5f;
            draw.RectFilled(Rect.FromXYWH(0.0f, 0.0f, size.x, size.y), Style.Color(StyleColor.ViewportBg), 4.0f);
            draw.Circle(center, radius, Style.Color(StyleColor.Grid));
            draw.Line(center, new Vector2(center.x + MathF.Cos(angle) * radius, center.y + MathF.Sin(angle) * radius), Style.Color(StyleColor.Accent), 3.0f);
            draw.CircleFilled(center, 4.0f, Style.Color(StyleColor.ActiveObject));
        }
    }
}
