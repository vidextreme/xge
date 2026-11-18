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

        // Reads View's input and sets the camera properties.
        protected abstract void UpdateView();

        protected override void OnGUI()
        {
            _state.TextureId = _view;
            Gui.SceneView("##render_view", ref _state, "No 3D renderer (RendererModule is empty)");
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
        }

        protected override void OnDestroy()
        {
            if (_view != 0)
                SceneViews.Destroy(_view);
            _view = 0;
        }
    }
}
