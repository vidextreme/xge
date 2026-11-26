// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg;
using xg.editor;
using xg.imui;
using xg.math;

namespace xgEditor.CoreCLR
{
    // The Scene view: a render view with an orbit camera (right-drag orbits, middle-drag pans, the wheel zooms). The left
    // button and the wheel go to the active tool first, through Tools.DispatchPointer; Tools.DrawScene draws its overlay.
    [PanelTool("Scene", Icon = "SceneGrid", Unique = true, Priority = 1, Tooltip = "Edit the scene")]
    public sealed class ScenePanel : RenderViewPanel, IPanelTool
    {
        private const float DragThreshold = 4.0f;

        private readonly SceneViewContext _context = new();
        private Vector3 _pivot = new(0.0f, 0.5f, 0.0f);
        private float _yaw = -1.2f;
        private float _pitch = 0.3f;
        private float _distance = 5.0f;
        private bool _leftDown;
        private bool _dragging;
        private Vector2 _pressPosition;
        private Vector2 _lastPosition;

        public override string Title => "Scene";
        public override string? MenuPath => "General/Scene";
        public override Icon PanelIcon => Icon.SceneGrid;
        protected override uint ViewFlags => ActorEditorState.ShowHidden ? 0u : SceneViews.FlagHideEditorHidden;

        public int InstanceIndex { get; set; }

        public ToolUsability OnSelectItems(object[] selection) => ToolUsability.Usable;

        protected override void UpdateView()
        {
            ref readonly SceneViewState view = ref View;
            float wheel = view.Wheel;
            ApplyCameraDrag(view);
            UpdateCamera();

            Tools? tools = Subsystem.Get<Tools>();
            if (tools is not null)
            {
                FillContext(view);
                if (DispatchPointer(tools, view))
                    wheel = 0.0f;
                tools.DrawScene(_context);
            }
            if (wheel != 0.0f)
            {
                _distance = Math.Clamp(_distance * MathF.Pow(0.9f, wheel), 0.5f, 200.0f);
                UpdateCamera();
            }
        }

        private void ApplyCameraDrag(in SceneViewState view)
        {
            _yaw += view.DragRight.x * 0.01f;
            _pitch = Math.Clamp(_pitch + view.DragRight.y * 0.01f, -1.5f, 1.5f);
            if (view.DragMiddle.x == 0.0f && view.DragMiddle.y == 0.0f)
                return;
            (float sinYaw, float cosYaw) = (MathF.Sin(_yaw), MathF.Cos(_yaw));
            (float sinPitch, float cosPitch) = (MathF.Sin(_pitch), MathF.Cos(_pitch));
            float scale = _distance * 0.002f;
            float dx = -view.DragMiddle.x * scale;
            float dy = view.DragMiddle.y * scale;
            _pivot = new Vector3(
                _pivot.x + cosYaw * dx - sinPitch * sinYaw * dy,
                _pivot.y + cosPitch * dy,
                _pivot.z + sinYaw * dx + sinPitch * cosYaw * dy);
        }

        private void UpdateCamera()
        {
            float cosPitch = MathF.Cos(_pitch);
            CameraTarget = _pivot;
            CameraEye = new Vector3(
                _pivot.x + _distance * cosPitch * MathF.Sin(_yaw),
                _pivot.y + _distance * MathF.Sin(_pitch),
                _pivot.z - _distance * cosPitch * MathF.Cos(_yaw));
        }

        private void FillContext(in SceneViewState view)
        {
            _context.ViewRect = Gui.ItemRect;
            _context.PixelSize = view.PixelSize;
            _context.CameraPosition = CameraEye;
            _context.CameraTarget = CameraTarget;
            _context.CameraUp = CameraUp;
            _context.FovY = CameraFovY;
            _context.NearZ = CameraNearZ;
            _context.FarZ = CameraFarZ;
        }

        // Returns true when the active tool used the wheel.
        private bool DispatchPointer(Tools tools, in SceneViewState view)
        {
            Vector2 position = view.MousePosition;
            Vector2 delta = new(position.x - _lastPosition.x, position.y - _lastPosition.y);
            bool moved = delta.x != 0.0f || delta.y != 0.0f;
            bool down = (view.Flags & SceneViewFlags.LeftDown) != 0;
            _lastPosition = position;

            if (moved && (view.Hovered || _leftDown))
                Send(tools, PointerEventType.Move, position, delta);
            if (down && !_leftDown)
            {
                _pressPosition = position;
                Send(tools, PointerEventType.Down, position, delta);
            }
            else if (down)
            {
                float fromPressX = position.x - _pressPosition.x;
                float fromPressY = position.y - _pressPosition.y;
                if (!_dragging && fromPressX * fromPressX + fromPressY * fromPressY >= DragThreshold * DragThreshold)
                {
                    _dragging = true;
                    Send(tools, PointerEventType.BeginDrag, position, delta);
                }
                else if (_dragging && moved)
                {
                    Send(tools, PointerEventType.Drag, position, delta);
                }
            }
            else if (_leftDown)
            {
                Send(tools, PointerEventType.Up, position, delta);
                Send(tools, _dragging ? PointerEventType.EndDrag : PointerEventType.Click, position, delta);
                _dragging = false;
            }
            _leftDown = down;

            return view.Wheel != 0.0f && Send(tools, PointerEventType.Scroll, position, delta, view.Wheel);
        }

        private bool Send(Tools tools, PointerEventType type, Vector2 position, Vector2 delta, float scroll = 0.0f)
        {
            PointerEventData data = new(_context)
            {
                Position = position,
                Delta = delta,
                Button = PointerButton.Left,
                ClickCount = type == PointerEventType.Click ? 1 : 0,
                Scroll = scroll,
            };
            tools.DispatchPointer(type, data);
            return data.Used;
        }
    }
}
