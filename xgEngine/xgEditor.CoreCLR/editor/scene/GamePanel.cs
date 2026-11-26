// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg;
using xg.editor;
using xg.imui;
using xg.math;

namespace xgEditor.CoreCLR
{
    // The Game view: renders through Camera.Main (the Main camera with the highest Priority, from any module): position
    // and orientation from its actor's world transform, its projection, clipping, background and viewport. Without a
    // main camera it shows the scene from a fixed camera and says so.
    [PanelTool("Game", Icon = "Game", Unique = true, Priority = 2, Tooltip = "View the game")]
    public sealed class GamePanel : RenderViewPanel, IPanelTool
    {
        private static readonly Vector3 _fixedEye = new(0.0f, 2.5f, -6.0f);
        private static readonly Vector3 _fixedTarget = new(0.0f, 0.5f, 0.0f);
        private static readonly Color _fixedClear = new(0.32f, 0.45f, 0.62f, 1.0f);

        private bool _hasCamera;
        private bool _noClear;

        public GamePanel()
        {
            CameraEye = _fixedEye;
            CameraTarget = _fixedTarget;
            ClearColor = _fixedClear;
        }

        public override string Title => "Game";
        public override string? MenuPath => "General/Game";
        public override Icon PanelIcon => Icon.Game;

        public int InstanceIndex { get; set; }

        // The camera actor the view renders through (InvalidID = the fixed camera).
        public ActorID CameraActor { get; private set; }

        public ToolUsability OnSelectItems(object[] selection) => ToolUsability.Usable;

        protected override uint ViewFlags => _noClear ? SceneViews.FlagNoClear : 0;

        protected override void UpdateView()
        {
            ActorID main = Camera.Main;
            CameraActor = main;
            _hasCamera = main.Value != 0 && Transform.Exists(main) && ComponentData.TryGet(main, out CameraData data);
            if (!_hasCamera || !ComponentData.TryGet(main, out data))
            {
                CameraEye = _fixedEye;
                CameraTarget = _fixedTarget;
                CameraUp = new Vector3(0.0f, 1.0f, 0.0f);
                CameraFovY = 0.9f;
                CameraNearZ = 0.1f;
                CameraFarZ = 500.0f;
                CameraOrthographicSize = 0.0f;
                ClearColor = _fixedClear;
                Viewport = new Vector4(0.0f, 0.0f, 1.0f, 1.0f);
                _noClear = false;
                return;
            }
            TransformData world = Transform.Of(main).World;
            Camera.ViewBasis(world, out Vector3 forward, out Vector3 up);
            CameraEye = world.Position;
            CameraTarget = new Vector3(world.PositionX + forward.x, world.PositionY + forward.y, world.PositionZ + forward.z);
            CameraUp = up;
            CameraFovY = data.VerticalFovRadians(Aspect);
            CameraNearZ = MathF.Max(data.Near, 0.0001f);
            CameraFarZ = MathF.Max(data.Far, CameraNearZ + 0.001f);
            CameraOrthographicSize = data.Projection == CameraProjection.Orthographic ? MathF.Max(data.OrthographicSize, 0.001f) : 0.0f;
            ClearColor = data.Background;
            Viewport = data.Viewport.z > 0.0f && data.Viewport.w > 0.0f ? data.Viewport : new Vector4(0.0f, 0.0f, 1.0f, 1.0f);
            _noClear = data.Clear == CameraClear.None;
        }

        protected override void DrawOverlay(Rect rect)
        {
            string text = _hasCamera ? ActorTree.GetName(CameraActor) : "No main camera (showing a fixed view)";
            Vector2 size = Gui.MeasureText(text);
            Painter draw = Gui.Draw;
            Vector2 at = new(rect.max.x - size.x - 10.0f, rect.max.y - size.y - 6.0f);
            draw.RectFilled(Rect.FromXYWH(at.x - 6.0f, at.y - 2.0f, size.x + 12.0f, size.y + 4.0f), new Color32(20, 21, 24, 170), 3.0f);
            draw.Text(at, Style.Color(_hasCamera ? StyleColor.TextDim : StyleColor.IconWarning), text);
        }
    }
}
