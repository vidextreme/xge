// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg.editor;
using xg.imui;

namespace xgEditor.CoreCLR
{
    // The Tools panel: hosts a Ribbon built by the tool system from the registered tools, buttons, toggles and tool GUIs,
    // rebuilt whenever tools are registered. It also drives the tool system's per-frame upkeep.
    public sealed class ToolsPanel : Panel
    {
        private readonly Ribbon _ribbon = new();
        private int _builtVersion = -1;

        public override string Title => "Tools";
        public override string? MenuPath => "General/Tools";
        public override DockSlot Slot =>
            Environment.GetEnvironmentVariable("XG_UI_TEST_RIBBON_DOCK") == "bottom" ? DockSlot.Bottom : DockSlot.Left;
        public override Icon PanelIcon => Icon.Gizmo;
        public override PanelFlags Flags => PanelFlags.NoPadding | PanelFlags.NoScrollbar;

        public Ribbon Ribbon => _ribbon;

        protected override void OnGUI()
        {
            Tools? tools = xg.Subsystem.Get<Tools>();
            if (tools is null)
            {
                Gui.Label("The tool system is not running.");
                return;
            }
            tools.Tick();
            if (tools.Version != _builtVersion)
            {
                tools.BuildRibbon(_ribbon);
                _builtVersion = tools.Version;
            }
            _ribbon.OnGUI();
        }
    }
}
