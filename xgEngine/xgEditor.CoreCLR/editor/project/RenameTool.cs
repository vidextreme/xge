// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg.editor;
using xg.imui;

namespace xgEditor.CoreCLR
{
    // Rename as a tool: a context menu entry for any menu opened on one asset (or folder), and a Tools ribbon button
    // for the single selected asset. Both open the shared RenameDialog.
    public sealed class RenameTool : IContextMenuTool
    {
        public string ContextMenuGroup => "Edit";
        public int ContextMenuPriority => 100;

        public ToolUsability OnContextMenu(ContextMenuContext context) =>
            context.Targets.Length == 1 && context.Targets[0] is AssetRef && ProjectService.Assets is not null
                ? ToolUsability.Usable : ToolUsability.NotApplicable;

        public void OnContextMenuGUI(ContextMenu menu, ContextMenuContext context)
        {
            AssetRef target = (AssetRef)context.Targets[0];
            bool exists = ProjectService.Assets?.Index.Find(target.Path) is not null;
            menu.Item("Rename...", Icon.File, () => RenameDialog.Open(target.Path), ContextMenu.EntryType.Button, exists);
        }

        [ToolButton("Rename", Category = "Scene", Group = "Assets", Icon = nameof(Icon.File), Priority = 30,
            Validate = nameof(SingleAsset), Tooltip = "Rename the selected asset")]
        public static void RenameSelected()
        {
            if (Selection.Assets.Count == 1)
                RenameDialog.Open(Selection.Assets[0].Path);
        }

        [ToolValidator]
        public static ToolUsability SingleAsset() =>
            ProjectService.Assets is not null && Selection.Assets.Count == 1 ? ToolUsability.Usable : ToolUsability.Unusable;
    }
}
