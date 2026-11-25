// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg;
using xg.editor;
using xg.imui;
using xg.math;

namespace xgEditor.CoreCLR
{
    // A composition object's preview: an offscreen render view (no test scene, no scene draws) that draws the driver's
    // draft (every Mesh Object's submeshes with their materials, placed by the Jointer chain), so edits show before
    // they're saved. Proxy axes, the selected Object's marker and names are drawn over it. Left or right drag orbits,
    // the wheel zooms; the settings icon at the top-left opens the preview menu.
    public sealed unsafe class ObjectPreview : IDisposable
    {
        private const float MenuWidth = 170.0f;
        private const float MenuRowHeight = 22.0f;
        private const float FovY = 0.6f;

        private readonly string _id;
        private readonly ObjectDrawList _drawList = new();
        private readonly List<(string Name, float[] World, bool Proxy, bool Selected)> _markers = new();
        private SceneViewState _state;
        private ulong _view;
        private int _width;
        private int _height;
        private float _yaw = 0.6f;
        private float _pitch = 0.35f;
        private float _zoom = 1.0f;
        private Vector3 _center;
        private float _radius = 1.0f;
        private Vector2 _menuPosition;
        private int _openMenuDelay = -1;

        public ObjectPreview(string id) => _id = id;

        public bool Checker { get; set; } = true;
        public bool AutoRotate { get; set; }
        public bool ShowProxies { get; set; } = true;
        public bool ShowNames { get; set; } = true;
        public int DrawCount { get; private set; }
        public int TriangleCount { get; private set; }

        // Opens the preview menu after the given number of frames (test hook).
        public void OpenMenu(int frames) => _openMenuDelay = frames;

        public void OnGUI(ObjectDriver? driver, string? selected, Vector2 size)
        {
            using var canvas = Gui.Canvas(_id, size);
            Painter draw = canvas.Draw;
            Rect area = Rect.FromXYWH(0.0f, 0.0f, size.x, size.y);
            DrawBackground(draw, area);
            using (var region = Gui.Region("##object_preview_region", area, out _))
            {
                if (region.IsOpen)
                {
                    _state.TextureId = _view;
                    Gui.SceneView("##object_preview_view", ref _state, "No 3D renderer", size);
                    UpdateCamera();
                    UpdateView(driver, selected);
                    DrawOverlay(size);
                    DrawMenuButton();
                    DrawMenu();
                }
            }
        }

        private void DrawBackground(Painter draw, Rect area)
        {
            draw.RectFilled(area, new Color32(46, 48, 54, 255), 4.0f);
            if (!Checker)
                return;
            const float cell = 12.0f;
            Color32 light = new(62, 64, 70, 255);
            for (int y = 0; y * cell < area.Height; y++)
                for (int x = (y & 1); x * cell < area.Width; x += 2)
                    draw.RectFilled(Rect.FromXYWH(x * cell, y * cell, MathF.Min(cell, area.Width - x * cell), MathF.Min(cell, area.Height - y * cell)), light);
        }

        private void DrawMenuButton()
        {
            Rect button = Rect.FromXYWH(4.0f, 4.0f, 22.0f, 22.0f);
            Result hit = Gui.HitArea("##object_preview_menu", button);
            Gui.Tooltip("Preview settings");
            Painter draw = Gui.Draw;
            Color32 fill = Style.Color(hit.Hovered() ? StyleColor.ButtonHover : StyleColor.Button);
            draw.RectFilled(button, new Color32(fill.r, fill.g, fill.b, 220), 2.0f);
            draw.Icon(Icon.Settings, button.Center, 14.0f, Style.Color(StyleColor.Text));
            if (hit.Clicked() || _openMenuDelay == 0)
            {
                _menuPosition = new Vector2(button.min.x, button.max.y + 2.0f);
                Gui.OpenPopup("object_preview_menu");
            }
            if (_openMenuDelay >= 0)
                _openMenuDelay--;
        }

        private void DrawMenu()
        {
            using var popup = Gui.Popup("object_preview_menu", _menuPosition, new Vector2(MenuWidth, 0.0f));
            if (!popup.IsOpen)
                return;
            float headerHeight = Gui.MeasureText("A").y + 6.0f;
            float height = 4.0f + headerHeight * 2.0f + (MenuRowHeight + 2.0f) * 6.0f + 6.0f;
            using var canvas = Gui.Canvas("##object_preview_menu_rows", new Vector2(MenuWidth, height));
            Painter draw = canvas.Draw;
            float inner = MenuWidth - 8.0f;
            float y = 4.0f;
            void Header(string text)
            {
                using (Gui.Font(FontStyle.Bold))
                    draw.Text(new Vector2(6.0f, y + 2.0f), Style.Color(StyleColor.TextDim), text);
                y += headerHeight;
            }
            bool Row(string id, Icon icon, string label, bool on, ToggleDot dot)
            {
                bool clicked = ToggleButton.Draw(draw, id, Rect.FromXYWH(4.0f, y, inner, MenuRowHeight), icon, null, label, null, null, on, true, dot).Clicked();
                y += MenuRowHeight + 2.0f;
                return clicked;
            }

            Header("Overlay");
            if (Row("##proxies", Icon.Gizmo, "Proxy axes", ShowProxies, ToggleDot.Check))
                ShowProxies = !ShowProxies;
            if (Row("##names", Icon.Tag, "Names", ShowNames, ToggleDot.Check))
                ShowNames = !ShowNames;
            Header("Background");
            if (Row("##checker", Icon.None, "Checker", Checker, ToggleDot.Radio))
                Checker = true;
            if (Row("##solid", Icon.None, "Solid", !Checker, ToggleDot.Radio))
                Checker = false;
            if (Row("##rotate", Icon.Rotate, "Auto-rotate", AutoRotate, ToggleDot.Check))
                AutoRotate = !AutoRotate;
            if (Row("##reset", Icon.Camera, "Reset camera", false, ToggleDot.None))
            {
                (_yaw, _pitch, _zoom) = (0.6f, 0.35f, 1.0f);
                Gui.ClosePopup();
            }
        }

        private void UpdateCamera()
        {
            Vector2 drag = new(_state.DragLeft.x + _state.DragRight.x, _state.DragLeft.y + _state.DragRight.y);
            _yaw += drag.x * 0.01f;
            _pitch = Math.Clamp(_pitch + drag.y * 0.01f, -1.4f, 1.4f);
            if (_state.Wheel != 0.0f)
                _zoom = Math.Clamp(_zoom * MathF.Pow(0.9f, _state.Wheel), 0.2f, 5.0f);
            if (AutoRotate && drag.x == 0.0f && drag.y == 0.0f)
                _yaw += 0.01f;
        }

        private Vector3 Eye()
        {
            float distance = _radius / MathF.Sin(FovY * 0.5f) * 1.1f * _zoom;
            float cosPitch = MathF.Cos(_pitch);
            return new Vector3(_center.x + MathF.Sin(_yaw) * cosPitch * distance, _center.y + MathF.Sin(_pitch) * distance,
                _center.z - MathF.Cos(_yaw) * cosPitch * distance);
        }

        private void UpdateView(ObjectDriver? driver, string? selected)
        {
            int width = _state.PixelWidth;
            int height = _state.PixelHeight;
            if (SceneViews.HasRenderer() == 0 || width <= 0 || height <= 0)
                return;
            if (_view == 0)
            {
                _view = SceneViews.Create(width, height);
                (_width, _height) = (width, height);
                if (_view != 0)
                    SceneViews.SetFlags(_view, SceneViews.FlagHideTestScene | SceneViews.FlagHideSceneDraws);
            }
            else if (width != _width || height != _height)
            {
                SceneViews.Resize(_view, width, height);
                (_width, _height) = (width, height);
            }
            if (_view == 0)
                return;

            BuildDraws(driver, selected);
            SceneViews.Camera camera = default;
            camera.Eye = Eye();
            camera.Target = _center;
            camera.Up = new Vector3(0.0f, 1.0f, 0.0f);
            camera.FovY = FovY;
            camera.NearZ = MathF.Max(_radius * 0.01f, 0.01f);
            camera.FarZ = MathF.Max(_radius * 40.0f, 50.0f);
            camera.ClearColor = new Color(0.0f, 0.0f, 0.0f, 0.0f);
            SceneViews.SetCamera(_view, &camera);
            _drawList.Submit(_view);
        }

        private void BuildDraws(ObjectDriver? driver, string? selected)
        {
            _markers.Clear();
            DrawCount = 0;
            TriangleCount = 0;
            if (driver is null)
            {
                _drawList.Dispose();
                return;
            }
            foreach (ObjectNodeAsset node in driver.Draft.Objects)
            {
                bool isSelected = string.Equals(node.Id, selected, StringComparison.OrdinalIgnoreCase);
                if (node.Kind == ObjectNodeKind.Proxy || isSelected)
                    _markers.Add((node.Name, driver.WorldOf(node), node.Kind == ObjectNodeKind.Proxy, isSelected));
            }
            _drawList.Build(driver.Draft);
            DrawCount = _drawList.Draws.Count;
            TriangleCount = _drawList.TriangleCount;
            if (_drawList.HasBounds)
            {
                _center = _drawList.Center;
                _radius = _drawList.Radius;
            }
        }

        private bool Project(Vector2 size, float x, float y, float z, out Vector2 screen)
        {
            screen = default;
            Vector3 eye = Eye();
            Vector3 forward = Normalize(new Vector3(_center.x - eye.x, _center.y - eye.y, _center.z - eye.z));
            Vector3 right = Normalize(Cross(new Vector3(0.0f, 1.0f, 0.0f), forward));
            Vector3 up = Cross(forward, right);
            Vector3 d = new(x - eye.x, y - eye.y, z - eye.z);
            float vz = Dot(d, forward);
            if (vz <= 1e-4f || size.y <= 0.0f)
                return false;
            float t = MathF.Tan(FovY * 0.5f);
            float aspect = size.x / size.y;
            float nx = Dot(d, right) / (vz * t * aspect);
            float ny = Dot(d, up) / (vz * t);
            screen = new Vector2((nx * 0.5f + 0.5f) * size.x, (0.5f - ny * 0.5f) * size.y);
            return true;
        }

        private void DrawOverlay(Vector2 size)
        {
            Painter draw = Gui.Draw;
            float axis = _radius * 0.18f;
            foreach ((string name, float[] world, bool proxy, bool selected) in _markers)
            {
                if (proxy && !ShowProxies && !selected)
                    continue;
                if (!Project(size, world[12], world[13], world[14], out Vector2 origin))
                    continue;
                (Color32 Color, float X, float Y, float Z)[] axes =
                {
                    (new Color32(224, 85, 85, 255), world[0], world[1], world[2]),
                    (new Color32(108, 192, 112, 255), world[4], world[5], world[6]),
                    (new Color32(90, 141, 224, 255), world[8], world[9], world[10]),
                };
                foreach ((Color32 color, float ax, float ay, float az) in axes)
                {
                    float length = MathF.Sqrt(ax * ax + ay * ay + az * az);
                    if (length <= 0.0f)
                        continue;
                    float s = axis / length;
                    if (Project(size, world[12] + ax * s, world[13] + ay * s, world[14] + az * s, out Vector2 tip))
                        draw.Line(origin, tip, color, selected ? 2.5f : 1.5f);
                }
                if (selected)
                    draw.Circle(origin, 6.0f, Style.Color(StyleColor.Accent), 2.0f);
                if (ShowNames || selected)
                {
                    Color32 text = selected ? Style.Color(StyleColor.Text) : new Color32(230, 160, 208, 230);
                    draw.Text(new Vector2(origin.x + 8.0f, origin.y - 16.0f), text, name);
                }
            }
            string stats = $"{_markers.Count(m => m.Proxy)} prox{(_markers.Count(m => m.Proxy) == 1 ? "y" : "ies")} · {DrawCount} draw(s) · {FormatCount(TriangleCount)} tris";
            Vector2 measure = Gui.MeasureText(stats);
            draw.Text(new Vector2(size.x - measure.x - 8.0f, size.y - measure.y - 4.0f), Style.Color(StyleColor.TextDim), stats);
        }

        private static string FormatCount(int count) => count >= 1000 ? $"{count / 1000.0f:0.#}k" : count.ToString();

        private static Vector3 Cross(Vector3 a, Vector3 b) => new(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
        private static float Dot(Vector3 a, Vector3 b) => a.x * b.x + a.y * b.y + a.z * b.z;

        private static Vector3 Normalize(Vector3 v)
        {
            float length = MathF.Sqrt(Dot(v, v));
            return length > 0.0f ? new Vector3(v.x / length, v.y / length, v.z / length) : new Vector3(0.0f, 0.0f, 1.0f);
        }

        public void Dispose()
        {
            if (_view != 0)
                SceneViews.Destroy(_view);
            _view = 0;
            _drawList.Dispose();
        }
    }
}
