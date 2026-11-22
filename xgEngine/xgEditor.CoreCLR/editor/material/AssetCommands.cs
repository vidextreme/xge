// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg;
using xg.editor;
using xg.imui;

[assembly: ToolGroup("Scene", "Assets", Width = 170.0f, Priority = 10, Tooltip = "Create and edit assets")]

namespace xgEditor.CoreCLR
{
    // The Tools ribbon's Scene > Assets group: create a material in the Navigator's folder, or edit the selected one.
    public static class AssetCommands
    {
        [ToolButton("Create material", Category = "Scene", Group = "Assets", Icon = "Material", Priority = 10,
            Validate = nameof(HasProject), Tooltip = "Create a material in the Navigator's folder and open it in the Material Editor")]
        [ContextMenuButton("Create material", Category = "Scene", Group = "Assets", Icon = "Material", Priority = 10)]
        public static void CreateMaterial()
        {
            string folder = ImUI.RegisteredPanels.OfType<NavigatorPanel>().FirstOrDefault()?.Folder ?? AssetIndex.RootFolder;
            if (AssetTemplates.CreateMaterial(folder) is not string path)
                return;
            Selection.Set(new AssetRef(path));
            MaterialEditorPanel.Open(path);
        }

        [ToolButton("Edit material", Category = "Scene", Group = "Assets", Icon = "Material", Priority = 20,
            Validate = nameof(MaterialSelected), Tooltip = "Open the selected material in the Material Editor")]
        public static void EditMaterial()
        {
            if (SelectedMaterial() is string path)
                MaterialEditorPanel.Open(path);
        }

        [ToolValidator]
        public static ToolUsability HasProject() => ProjectService.Assets is not null ? ToolUsability.Usable : ToolUsability.Unusable;

        [ToolValidator]
        public static ToolUsability MaterialSelected() => SelectedMaterial() is not null ? ToolUsability.Usable : ToolUsability.Unusable;

        private static string? SelectedMaterial() =>
            Selection.Objects.LastOrDefault() is AssetRef asset && MaterialAsset.IsMaterial(asset.Path) ? asset.Path : null;
    }
}
