// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg.editor;
using xg.imui;
using xg.math;

namespace xgEditor.CoreCLR
{
    // The Game view: a render view from a fixed camera until the engine has game cameras.
    [PanelTool("Game", Icon = "Game", Unique = true, Priority = 2, Tooltip = "View the game")]
    public sealed class GamePanel : RenderViewPanel, IPanelTool
    {
        public GamePanel()
        {
            CameraEye = new Vector3(0.0f, 2.5f, -6.0f);
            CameraTarget = new Vector3(0.0f, 0.5f, 0.0f);
            ClearColor = new Color(0.32f, 0.45f, 0.62f, 1.0f);
        }

        public override string Title => "Game";
        public override string? MenuPath => "General/Game";
        public override Icon PanelIcon => Icon.Game;

        public int InstanceIndex { get; set; }

        public ToolUsability OnSelectItems(object[] selection) => ToolUsability.Usable;

        protected override void UpdateView()
        {
        }
    }
}
