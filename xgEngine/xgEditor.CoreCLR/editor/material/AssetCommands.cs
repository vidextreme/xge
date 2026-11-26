// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg;
using xg.editor;
using xg.imui;

[assembly: ToolGroup("Scene", "Assets", Width = 170.0f, Priority = 10, Tooltip = "Create and edit assets")]

namespace xgEditor.CoreCLR
{
    // The Tools ribbon's Scene > Assets group: create a material in the Navigator's folder, or edit the selected one.
    public static partial class AssetCommands
    {
        [ToolButton("Create Material", Category = "Scene", Group = "Assets", Icon = nameof(Icon.Material), Priority = 10,
            Validate = nameof(HasProject), Tooltip = "Create a material in the Navigator's folder and open it in the Material Editor")]
        [ContextMenuButton("Create Material", Sources = "navigator", Category = "Scene", Group = "Assets", Icon = nameof(Icon.Material), Priority = 10,
            Validate = nameof(HasProject), Tooltip = "Create a material in the Navigator's folder and open it in the Material Editor")]
        public static void CreateMaterial()
        {
            NavigatorPanel? navigator = ImUI.RegisteredPanels.OfType<NavigatorPanel>().FirstOrDefault();
            string folder = navigator?.Folder ?? AssetIndex.RootFolder;            
            if (AssetTemplates.CreateMaterial(folder) is not string path)
                return;
            navigator?.RenameCreated(path, ()=> 
                {
                    Selection.Set(path);
                    MaterialEditorPanel.Open(path);
                });
        }

        [ToolButton("Edit Material", Category = "Scene", Group = "Assets", Icon = nameof(Icon.Material), Priority = 20,
            Validate = nameof(MaterialSelected), Tooltip = "Open the selected material in the Material Editor")]
        [ContextMenuButton("Edit Material", Sources = "navigator", Category = "Scene", Group = "Assets", Icon = nameof(Icon.Material), Priority = 20,
            Validate = nameof(MaterialSelected), Tooltip = "Open the selected material in the Material Editor")]
        public static void EditMaterial()
        {
            if (SelectedMaterial() is string path)
                MaterialEditorPanel.Open(path);
        }

        [MenuSeparator]
        [ToolButton("Create Shader", Category = "Scene", Group = "Assets", Icon = nameof(Icon.Code), Priority = 10,
            Validate = nameof(HasProject), Tooltip = "Create a new shader.")]
        [ContextMenuButton("Create Shader", Sources = "navigator", Category = "Scene", Group = "Assets", Icon = nameof(Icon.Code), Priority = 10,
            Validate = nameof(HasProject), Tooltip = "Create a new shader.")]
        public static void CreateShader()
        {
            NavigatorPanel? navigator = ImUI.RegisteredPanels.OfType<NavigatorPanel>().FirstOrDefault();
            string folder = navigator?.Folder ?? AssetIndex.RootFolder;
            if (AssetTemplates.CreateShader(folder) is not string path)
                return;
            navigator?.RenameCreated(path, () =>
            {
                Selection.Set(path);
                MaterialEditorPanel.Open(path);
            });
        }

        [MenuSeparator]
        [ToolButton("Create Scene", Category = "Scene", Group = "Assets", Icon = nameof(Icon.Scene), Priority = 28,
            Validate = nameof(HasProject), Tooltip = "Create an empty scene (.xcs) in the Navigator's folder")]
        [ContextMenuButton("Create Scene", Sources = "navigator", Category = "Scene", Group = "Assets", Icon = nameof(Icon.Scene), Priority = 28,
            Validate = nameof(HasProject), Tooltip = "Create an empty scene (.xcs) in the Navigator's folder")]
        public static void CreateScene()
        {
            NavigatorPanel? navigator = ImUI.RegisteredPanels.OfType<NavigatorPanel>().FirstOrDefault();
            string folder = navigator?.Folder ?? AssetIndex.RootFolder;
            if (AssetTemplates.CreateScene(folder) is not string path)
                return;
            navigator?.RenameCreated(path, () => Selection.Set(path));
        }

        [ToolButton("Create Object", Category = "Scene", Group = "Assets", Icon = nameof(Icon.Object), Priority = 30,
            Validate = nameof(HasProject), Tooltip = "Create a composition object in the Navigator's folder")]
        [ContextMenuButton("Create Object", Sources = "navigator", Category = "Scene", Group = "Assets", Icon = nameof(Icon.Object), Priority = 30,
            Validate = nameof(HasProject), Tooltip = "Create a composition object in the Navigator's folder")]
        public static void CreateObject()
        {
            NavigatorPanel? navigator = ImUI.RegisteredPanels.OfType<NavigatorPanel>().FirstOrDefault();
            string folder = navigator?.Folder ?? AssetIndex.RootFolder;
            if (AssetTemplates.CreateObject(folder) is not string path)
                return;
            navigator?.RenameCreated(path, () =>
            {
                Selection.Set(path);
                ObjectEditorPanel.Open(path);
            });
        }

        [ToolButton("Edit Object", Category = "Scene", Group = "Assets", Icon = nameof(Icon.Object), Priority = 35,
            Validate = nameof(ObjectSelected), Tooltip = "Open the selected composition object in the Object Editor")]
        [ContextMenuButton("Edit Object", Sources = "navigator", Category = "Scene", Group = "Assets", Icon = nameof(Icon.Object), Priority = 35,
            Validate = nameof(ObjectSelected), Tooltip = "Open the selected composition object in the Object Editor")]
        public static void EditObject()
        {
            if (Selection.Objects.LastOrDefault() is AssetRef asset && ObjectAsset.IsObject(asset.Path))
                ObjectEditorPanel.Open(asset.Path);
        }

        [ToolValidator]
        public static ToolUsability ObjectSelected() =>
            Selection.Objects.LastOrDefault() is AssetRef asset && ObjectAsset.IsObject(asset.Path) ? ToolUsability.Usable : ToolUsability.Unusable;

        [ToolButton("Object from Model", Category = "Scene", Group = "Assets", Icon = nameof(Icon.Object), Priority = 40,
            Validate = nameof(ModelSelected), Tooltip = "Create a composition object from the selected model, one Mesh Object per mesh")]
        [ContextMenuButton("Object from Model", Sources = "navigator", Category = "Scene", Group = "Assets", Icon = nameof(Icon.Object), Priority = 40,
            Validate = nameof(ModelSelected), Tooltip = "Create a composition object from the selected model, one Mesh Object per mesh")]
        public static void CreateObjectFromModel()
        {
            if (SelectedModel() is not string model || AssetTemplates.CreateObjectFromModel(model) is not string path)
                return;
            NavigatorPanel? navigator = ImUI.RegisteredPanels.OfType<NavigatorPanel>().FirstOrDefault();
            Debug.Log($"[Objects] Created {path} from {model}");
            void Opened()
            {
                Selection.Set(path);
                ObjectEditorPanel.Open(path);
            }
            if (navigator is not null)
                navigator.RenameCreated(path, Opened);
            else
                Opened();
        }

        [ToolValidator]
        public static ToolUsability ModelSelected() => SelectedModel() is not null ? ToolUsability.Usable : ToolUsability.Unusable;

        private static string? SelectedModel() =>
            Selection.Objects.LastOrDefault() is AssetRef asset && AssetIndex.KindOf(Path.GetExtension(asset.Path).TrimStart('.')) == AssetKind.Mesh
                && ProjectService.Assets?.Pipeline.CanImport(asset.Path) == true ? asset.Path : null;

        [ToolValidator]
        public static ToolUsability HasProject() => ProjectService.Assets is not null ? ToolUsability.Usable : ToolUsability.Unusable;

        [ToolValidator]
        public static ToolUsability MaterialSelected() => SelectedMaterial() is not null ? ToolUsability.Usable : ToolUsability.Unusable;

        private static string? SelectedMaterial() =>
            Selection.Objects.LastOrDefault() is AssetRef asset && MaterialAsset.IsMaterial(asset.Path) ? asset.Path : null;
    }
}
