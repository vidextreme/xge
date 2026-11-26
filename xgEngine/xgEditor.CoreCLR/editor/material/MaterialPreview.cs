// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg;
using xg.editor;
using xg.imui;
using xg.math;

namespace xgEditor.CoreCLR
{
    // A material's preview: an offscreen render view (no test scene, no scene draws) that draws a built-in shape with a
    // transient renderer material made from the driver's draft, so edits show before they're saved. Left or right drag
    // orbits, the wheel zooms. The settings icon at the top-left opens the preview menu.
    public sealed unsafe class MaterialPreview : IDisposable
    {
        private const float MenuWidth = 170.0f;
        private const float MenuRowHeight = 22.0f;

        private readonly string _id;
        private readonly Dictionary<string, ResourceHandle> _textures = new(StringComparer.Ordinal);
        private SceneViewState _state;
        private ulong _view;
        private int _width;
        private int _height;
        private ulong _material;
        private string _shaderGuid = string.Empty;
        private ResourceHandle _shader;
        private ulong _builtShader;
        private int _builtVersion = -1;
        private readonly ulong[] _builtTextures = new ulong[4];
        private float _yaw = 0.6f;
        private float _pitch = 0.35f;
        private float _distance = 3.5f;
        private Vector2 _menuPosition;
        private int _openMenuDelay = -1;

        public MaterialPreview(string id) => _id = id;

        // The renderer material built from the driver's draft (0 until one was built).
        public ulong DraftMaterial => _material;

        // Rebuilds the draft material without drawing (hosts that show the draft elsewhere while the preview is folded).
        public void RefreshDraft(MaterialDriver? driver)
        {
            if (SceneViews.HasRenderer() != 0)
                UpdateMaterial(driver);
        }

        public PreviewShape Shape { get; set; } = PreviewShape.Sphere;
        public bool Checker { get; set; } = true;
        public bool AutoRotate { get; set; }

        // Opens the preview menu after the given number of frames (test hook).
        public void OpenMenu(int frames) => _openMenuDelay = frames;

        public void OnGUI(MaterialDriver? driver, Vector2 size)
        {
            using var canvas = Gui.Canvas(_id, size);
            Painter draw = canvas.Draw;
            Rect area = Rect.FromXYWH(0.0f, 0.0f, size.x, size.y);
            DrawBackground(draw, area);
            using (var region = Gui.Region("##preview_region", area, out _))
            {
                if (region.IsOpen)
                {
                    _state.TextureId = _view;
                    Gui.SceneView("##preview_view", ref _state, "No 3D renderer", size);
                    UpdateCamera();
                    UpdateView(driver);
                    DrawMenuButton();
                    DrawMenu();
                }
            }
        }

        // The settings button sits in the region, after the scene view, so it wins the click over it.
        private void DrawMenuButton()
        {
            Rect button = Rect.FromXYWH(4.0f, 4.0f, 22.0f, 22.0f);
            Result hit = Gui.HitArea("##preview_menu", button);
            Gui.Tooltip("Preview settings");
            Painter draw = Gui.Draw;
            Color32 fill = Style.Color(hit.Hovered() ? StyleColor.ButtonHover : StyleColor.Button);
            draw.RectFilled(button, new Color32(fill.r, fill.g, fill.b, 220), 2.0f);
            draw.Icon(Icon.Settings, button.Center, 14.0f, Style.Color(StyleColor.Text));
            if (hit.Clicked() || _openMenuDelay == 0)
            {
                _menuPosition = new Vector2(button.min.x, button.max.y + 2.0f);
                Gui.OpenPopup("material_preview_menu");
            }
            if (_openMenuDelay >= 0)
                _openMenuDelay--;
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

        private void DrawMenu()
        {
            using var popup = Gui.Popup("material_preview_menu", _menuPosition, new Vector2(MenuWidth, 0.0f));
            if (!popup.IsOpen)
                return;
            float lineHeight = Gui.MeasureText("A").y;
            float headerHeight = lineHeight + 6.0f;
            float height = 4.0f + headerHeight * 2.0f + (MenuRowHeight + 2.0f) * 7.0f + 6.0f;
            using var canvas = Gui.Canvas("##preview_menu_rows", new Vector2(MenuWidth, height));
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

            Header("Shape");
            foreach (PreviewShape shape in Enum.GetValues<PreviewShape>())
                if (Row("##shape_" + shape, Icon.None, shape.ToString(), Shape == shape, ToggleDot.Radio))
                    Shape = shape;
            Header("Background");
            if (Row("##checker", Icon.None, "Checker", Checker, ToggleDot.Radio))
                Checker = true;
            if (Row("##solid", Icon.None, "Solid", !Checker, ToggleDot.Radio))
                Checker = false;
            y += 4.0f;
            if (Row("##rotate", Icon.Rotate, "Auto-Rotate", AutoRotate, ToggleDot.Check))
                AutoRotate = !AutoRotate;
            if (Row("##reset", Icon.Camera, "Reset Camera", false, ToggleDot.None))
            {
                (_yaw, _pitch, _distance) = (0.6f, 0.35f, 3.0f);
                Gui.ClosePopup();
            }
        }

        private void UpdateCamera()
        {
            Vector2 drag = new(_state.DragLeft.x + _state.DragRight.x, _state.DragLeft.y + _state.DragRight.y);
            _yaw += drag.x * 0.01f;
            _pitch = Math.Clamp(_pitch + drag.y * 0.01f, -1.4f, 1.4f);
            if (_state.Wheel != 0.0f)
                _distance = Math.Clamp(_distance * MathF.Pow(0.9f, _state.Wheel), 1.5f, 10.0f);
            if (AutoRotate && drag.x == 0.0f && drag.y == 0.0f)
                _yaw += 0.01f;
        }

        private void UpdateView(MaterialDriver? driver)
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

            SceneViews.Camera camera = default;
            float cosPitch = MathF.Cos(_pitch);
            camera.Eye = new Vector3(MathF.Sin(_yaw) * cosPitch * _distance, MathF.Sin(_pitch) * _distance, -MathF.Cos(_yaw) * cosPitch * _distance);
            camera.Target = new Vector3(0.0f, 0.0f, 0.0f);
            camera.Up = new Vector3(0.0f, 1.0f, 0.0f);
            camera.FovY = 0.6f;
            camera.NearZ = 0.05f;
            camera.FarZ = 50.0f;
            camera.ClearColor = new Color(0.0f, 0.0f, 0.0f, 0.0f);
            SceneViews.SetCamera(_view, &camera);

            ulong mesh = PreviewShapes.Get(Shape);
            UpdateMaterial(driver);
            SceneViews.MaterialDraw shapeDraw = default;
            shapeDraw.Mesh = mesh;
            shapeDraw.Material = _material;
            for (int i = 0; i < 16; i++)
                shapeDraw.World[i] = i % 5 == 0 ? 1.0f : 0.0f;
            SceneViews.SetMaterialDraws(_view, &shapeDraw, mesh != 0 ? 1u : 0u);
        }

        private ResourceHandle AcquireTexture(string guid)
        {
            if (_textures.TryGetValue(guid, out ResourceHandle handle) && !Resources.IsStale(handle))
                return handle;
            if (handle.Value != 0)
                handle.Dispose();
            uint type = Resources.FindType(Resources.TextureType);
            handle = type != 0 && ResourceID.TryParse(guid, out ResourceID id) ? Resources.Acquire(type, id) : default;
            _textures[guid] = handle;
            return handle;
        }

        private void UpdateMaterial(MaterialDriver? driver)
        {
            string shaderGuid = driver?.Draft.Shader ?? string.Empty;
            if (shaderGuid != _shaderGuid || (_shader.Value != 0 && Resources.IsStale(_shader)))
            {
                if (_shader.Value != 0)
                    _shader.Dispose();
                _shader = default;
                _shaderGuid = shaderGuid;
                uint type = Resources.FindType(Resources.ShaderType);
                if (type != 0 && ResourceID.TryParse(shaderGuid, out ResourceID id))
                    _shader = Resources.Acquire(type, id);
            }
            ulong shaderId = _shader.Value != 0 && _shader.State == ResourceState.Ready ? Resources.GetShader(_shader) : 0;

            SceneViews.MaterialDesc desc = default;
            desc.Shader = shaderId;
            ShaderAnnotations? annotations = driver?.Annotations;
            bool texturesChanged = false;
            if (driver is not null && annotations is not null)
            {
                foreach (ShaderParam param in annotations.Params)
                {
                    float[] value = driver.ParamValue(param);
                    for (int c = 0; c < 4; c++)
                        desc.Params[param.Slot * 4 + c] = c < value.Length ? value[c] : 0.0f;
                }
                foreach (ShaderTexture texture in annotations.Textures)
                {
                    string guid = driver.TextureGuid(texture);
                    if (guid.Length == 0)
                        continue;
                    ResourceHandle handle = AcquireTexture(guid);
                    if (handle.Value != 0 && handle.State == ResourceState.Ready && Resources.GetTexture(handle, out TextureInfo info))
                        desc.Textures[texture.Slot] = info.TextureId;
                }
            }
            for (int i = 0; i < 4; i++)
            {
                texturesChanged |= desc.Textures[i] != _builtTextures[i];
                _builtTextures[i] = desc.Textures[i];
            }
            int version = driver?.DraftVersion ?? 0;
            if (_material != 0 && shaderId == _builtShader && version == _builtVersion && !texturesChanged)
                return;
            _builtShader = shaderId;
            _builtVersion = version;
            if (_material == 0)
                _material = SceneViews.CreateMaterial(&desc);
            else if (SceneViews.UpdateMaterial(_material, &desc) == 0)
                _material = SceneViews.CreateMaterial(&desc);
        }

        public void Dispose()
        {
            if (_view != 0)
                SceneViews.Destroy(_view);
            _view = 0;
            if (_material != 0)
                SceneViews.DestroyMaterial(_material);
            _material = 0;
            if (_shader.Value != 0)
                _shader.Dispose();
            _shader = default;
            foreach (ResourceHandle handle in _textures.Values)
                if (handle.Value != 0)
                    handle.Dispose();
            _textures.Clear();
        }
    }
}
