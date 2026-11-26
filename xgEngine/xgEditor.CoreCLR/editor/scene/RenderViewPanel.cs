// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg.imui;
using xg.math;

namespace xgEditor.CoreCLR
{
    // A panel showing an offscreen render view: owns the view, follows the panel's size and sends the camera each frame.
    public abstract unsafe class RenderViewPanel : Panel
    {
        private SceneViewState _state;
        private ulong _view;
        private int _width;
        private int _height;
        private ulong _texture;
        private int _textureMode;
        private ulong _textureView;
        private SceneViews.Draw[] _draws = Array.Empty<SceneViews.Draw>();
        private ulong _drawsView;
        private uint _sentFlags = uint.MaxValue;
        private ulong _flagsView;
        private float _sentOrthographic = -1.0f;
        private ulong _projectionView;

        public override DockSlot Slot => DockSlot.Center;
        public override PanelFlags Flags => PanelFlags.NoPadding | PanelFlags.NoScrollbar;

        protected ref readonly SceneViewState View => ref _state;
        protected Vector3 CameraEye { get; set; }
        protected Vector3 CameraTarget { get; set; }
        protected Vector3 CameraUp { get; set; } = new(0.0f, 1.0f, 0.0f);
        protected float CameraFovY { get; set; } = 0.9f;
        protected float CameraNearZ { get; set; } = 0.1f;
        protected float CameraFarZ { get; set; } = 500.0f;
        protected Color ClearColor { get; set; } = new(0.16f, 0.18f, 0.22f, 1.0f);
        // Half the view's height in world units for an orthographic camera; 0 = perspective.
        protected float CameraOrthographicSize { get; set; }
        // The part of the panel the view covers (x, y from the top-left, width, height, in 0..1).
        protected Vector4 Viewport { get; set; } = new(0.0f, 0.0f, 1.0f, 1.0f);
        // Width / height of the view in pixels (last frame).
        protected float Aspect => _state.PixelHeight > 0 ? (float)_state.PixelWidth / _state.PixelHeight : 1.0f;

        // Reads View's input and sets the camera properties.
        protected abstract void UpdateView();

        // SceneViews.Flag* bits sent to the view (re-sent when they change).
        protected virtual uint ViewFlags => 0;

        // Draws over the view (rect = the view in draw space).
        protected virtual void DrawOverlay(Rect rect)
        {
        }

        // Meshes drawn in the view after the test cube (empty clears).
        internal void SetDraws(SceneViews.Draw[] draws)
        {
            _draws = draws;
            _drawsView = 0;
        }

        // Shows a renderer texture on the view's test cube (0 = none). mode: 0 color, 1 normal, 2 luminance.
        public void SetTexture(ulong textureId, int mode)
        {
            _texture = textureId;
            _textureMode = mode;
            _textureView = 0;
        }

        protected override void OnGUI()
        {
            _state.TextureId = _view;
            Vector4 viewport = Viewport;
            if (viewport.x <= 0.0f && viewport.y <= 0.0f && viewport.z >= 1.0f && viewport.w >= 1.0f)
            {
                Gui.SceneView("##render_view", ref _state, "No 3D renderer (RendererModule is empty)");
                DrawOverlay(Gui.ItemRect);
            }
            else
            {
                DrawViewport(viewport);
            }
            UpdateView();

            int width = _state.PixelWidth;
            int height = _state.PixelHeight;
            if (SceneViews.HasRenderer() == 0 || width <= 0 || height <= 0)
                return;

            if (_view == 0)
            {
                _view = SceneViews.Create(width, height);
                (_width, _height) = (width, height);
            }
            else if (width != _width || height != _height)
            {
                SceneViews.Resize(_view, width, height);
                (_width, _height) = (width, height);
            }
            if (_view == 0)
                return;

            SceneViews.Camera camera = default;
            camera.Eye = CameraEye;
            camera.Target = CameraTarget;
            camera.Up = CameraUp;
            camera.FovY = CameraFovY;
            camera.NearZ = CameraNearZ;
            camera.FarZ = CameraFarZ;
            camera.ClearColor = ClearColor;
            SceneViews.SetCamera(_view, &camera);
            if (CameraOrthographicSize != _sentOrthographic || _projectionView != _view)
            {
                SceneViews.SetProjection(_view, CameraOrthographicSize);
                (_sentOrthographic, _projectionView) = (CameraOrthographicSize, _view);
            }
            uint flags = ViewFlags;
            if (flags != _sentFlags || _flagsView != _view)
            {
                SceneViews.SetFlags(_view, flags);
                (_sentFlags, _flagsView) = (flags, _view);
            }
            if (_textureView != _view)
            {
                SceneViews.SetTexture(_view, _texture, _textureMode);
                _textureView = _view;
            }
            if (_drawsView != _view)
            {
                fixed (SceneViews.Draw* draws = _draws)
                    SceneViews.SetDraws(_view, draws, (uint)_draws.Length);
                _drawsView = _view;
            }
        }

        // The view inside a part of the panel; the rest is a dark letterbox.
        private void DrawViewport(Vector4 viewport)
        {
            Vector2 size = Gui.AvailableSize;
            using var canvas = Gui.Canvas("##viewport_area", size);
            canvas.Draw.RectFilled(Rect.FromXYWH(0.0f, 0.0f, size.x, size.y), new Color32(14, 15, 17, 255));
            float x = Math.Clamp(viewport.x, 0.0f, 1.0f) * size.x;
            float y = Math.Clamp(viewport.y, 0.0f, 1.0f) * size.y;
            float w = MathF.Max(Math.Clamp(viewport.z, 0.0f, 1.0f) * size.x, 8.0f);
            float h = MathF.Max(Math.Clamp(viewport.w, 0.0f, 1.0f) * size.y, 8.0f);
            Rect rect = Rect.FromXYWH(x, y, MathF.Min(w, size.x - x), MathF.Min(h, size.y - y));
            using var region = Gui.Region("##viewport", rect, out _);
            if (!region.IsOpen)
                return;
            Gui.SceneView("##render_view", ref _state, "No 3D renderer (RendererModule is empty)", rect.Size);
            DrawOverlay(Gui.ItemRect);
        }

        protected override void OnDestroy()
        {
            if (_view != 0)
                SceneViews.Destroy(_view);
            _view = 0;
        }
    }
}
