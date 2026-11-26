// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

namespace xg.editor
{
    // Selection buttons and toggles, independent of any tool: they declare their own place in the ribbon.
    public static class SelectionCommands
    {
        public const string SelectChildrenKey = "xg.editor.SelectionCommands.SelectChildren";

        public static bool SelectChildrenEnabled => Subsystem.Get<Tools>()?.GetToggle(SelectChildrenKey) ?? false;

        [ToolButton("Clear Selection", Category = "Scene", Group = "Select", Icon = "Trash", Priority = 10,
            Validate = nameof(HasSelection), AllowLocked = true, Tooltip = "Deselect every actor and asset")]
        public static void ClearSelection() => Selection.Clear();

        [ToolToggle("Select Children", Category = "Scene", Group = "Select", Icon = "Hierarchy", Priority = 20, AllowLocked = true,
            Tooltip = "Selecting an actor in the Hierarchy also selects its children (coming with multi-selection)")]
        public static bool SelectChildren(bool requested) => requested;

        [ToolValidator]
        public static ToolUsability HasSelection() => Selection.Count > 0 ? ToolUsability.Usable : ToolUsability.Unusable;
    }
}
