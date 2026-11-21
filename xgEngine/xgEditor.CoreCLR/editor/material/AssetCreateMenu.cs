// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg.editor;
using xg.imui;

namespace xgEditor.CoreCLR
{
    // The Navigator's background menu: new assets in the folder it shows, then the Rename modal.
    public sealed class AssetCreateMenu : IContextMenuTool
    {
        public string ContextMenuGroup => "Create";

        public ToolUsability OnContextMenu(ContextMenuContext context) =>
            context.Source == NavigatorPanel.BackgroundMenu && context.Get<NavigatorPanel>() is not null
                ? ToolUsability.Usable : ToolUsability.NotApplicable;

        public void OnContextMenuGUI(ContextMenu menu, ContextMenuContext context)
        {
            NavigatorPanel navigator = context.Get<NavigatorPanel>()!;
            string folder = navigator.Folder;
            menu.Item("Create material", Icon.Material, () => Created(navigator, AssetTemplates.CreateMaterial(folder)), tooltip: $"A new .xcm in {folder}");
            menu.Item("Create shader", Icon.Code, () => Created(navigator, AssetTemplates.CreateShader(folder)), tooltip: $"A new surface shader in {folder}");
        }

        private static void Created(NavigatorPanel navigator, string? path)
        {
            if (path is not null)
                navigator.RenameCreated(path);
        }
    }
}
