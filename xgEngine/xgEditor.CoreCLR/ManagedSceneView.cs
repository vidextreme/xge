// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg;
using xg.imui;
using xg.math;

namespace xgEditor.CoreCLR
{
    // Test hook: a managed panel owning an offscreen 3D view with an orbit camera. Mirrors the native "Scene View" panels.
    // Closed by default; XG_UI_TEST_FOCUS=scene opens it at startup.
    internal sealed unsafe class ManagedSceneViewWindow : Panel
    {
        private SceneViewState _state;
        private ulong _view;
        private int _width;
        private int _height;
        private float _yaw = -1.2f;
        private float _pitch = 0.3f;
        private float _distance = 4.0f;
        private bool _reported;

        public override string Title => "Managed Scene View";
        public override string? MenuPath => "Tests/Managed Scene View";
        public override DockSlot Slot => DockSlot.Floating;
        public override Icon PanelIcon => Icon.SceneGrid;
        public override PanelFlags Flags =>
            PanelFlags.NoPadding | PanelFlags.NoScrollbar |
            (Environment.GetEnvironmentVariable("XG_UI_TEST_FOCUS") == "scene" ? PanelFlags.None : PanelFlags.StartClosed);
        public override Rect? InitialRect => Rect.FromXYWH(860.0f, 330.0f, 400.0f, 300.0f);

        protected override void OnGUI()
        {
            _state.TextureId = _view;
            Gui.SceneView("##managed_scene_view", ref _state, "No 3D renderer (RendererModule is empty)");

            _yaw += _state.DragLeft.x * 0.01f;
            _pitch = Math.Clamp(_pitch + _state.DragLeft.y * 0.01f, -1.4f, 1.4f);
            _distance = Math.Clamp(_distance * MathF.Pow(0.9f, _state.Wheel), 1.5f, 40.0f);

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
            camera.Target = new Vector3(0.0f, 0.5f, 0.0f);
            camera.Eye = new Vector3(_distance * MathF.Cos(_pitch) * MathF.Sin(_yaw), 0.5f + _distance * MathF.Sin(_pitch), -_distance * MathF.Cos(_pitch) * MathF.Cos(_yaw));
            camera.Up = new Vector3(0.0f, 1.0f, 0.0f);
            camera.FovY = 0.9f;
            camera.NearZ = 0.1f;
            camera.FarZ = 100.0f;
            camera.ClearColor = new Color(0.14f, 0.2f, 0.16f, 1.0f);
            SceneViews.SetCamera(_view, &camera);

            if (!_reported && width > 64)
            {
                _reported = true;
                Debug.Log($"[Debug] Managed Scene View: {width}x{height}, texture {_view}");
            }
        }

        protected override void OnDestroy()
        {
            if (_view != 0)
                SceneViews.Destroy(_view);
            _view = 0;
        }
    }
}
