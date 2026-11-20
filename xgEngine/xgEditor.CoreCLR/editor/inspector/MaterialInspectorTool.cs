// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg;
using xg.editor;
using xg.imui;
using xg.math;

namespace xgEditor.CoreCLR
{
    // A summary of the selected .xcm material (shader, groups, tags) with a button that opens it in the Material Editor,
    // where it's edited.
    [InspectorTool("Material", Icon = "Material", Priority = 0, Tooltip = "The selected material")]
    public sealed class MaterialInspectorTool : IInspectorTool
    {
        public ToolUsability OnSelectItems(object[] selection) =>
            selection.LastOrDefault() is AssetRef asset && MaterialAsset.IsMaterial(asset.Path) ? ToolUsability.Usable : ToolUsability.NotApplicable;

        public object? DriverKey(ToolSelection selection) => selection.ActiveObject as AssetRef?;

        public IToolDriver? CreateDriver(ToolSelection selection) =>
            selection.ActiveObject is AssetRef asset ? new MaterialDriver(asset.Path) : null;

        public void OnInspectorGUI(InspectorToolContext context)
        {
            MaterialDriver? driver = context.Driver as MaterialDriver;
            using var area = Gui.ScrollArea("##material_inspector", default, ScrollAreaFlags.Padded);
            if (!area.IsOpen)
                return;
            Vector2 avail = Gui.AvailableSize;
            Gui.Draw.RectFilled(new Rect(new Vector2(-6.0f, -6.0f), new Vector2(avail.x + 6.0f, 100000.0f)), Style.Color(StyleColor.ToolbarBg));
            using (Gui.Horizontal())
            {
                using (Gui.Font(FontStyle.Bold))
                    Gui.Label(driver?.Name ?? "Material");
                Gui.FlexibleSpace();
                context.DrawLock();
            }
            if (driver is null)
                return;
            if (driver.Error is not null)
            {
                Gui.Label(driver.Error);
                return;
            }
            driver.RefreshAnnotations();
            using (Gui.Card("##material_summary", "Material", Icon.Material, null, Style.Color(StyleColor.IconMaterial)))
            {
                Gui.PropertyLabel("Shader", driver.ShaderPath is null ? AssetBrowser.NameOf(driver.Draft.Shader) : Path.GetFileName(driver.ShaderPath));
                if (driver.Annotations is { } annotations)
                {
                    foreach (string group in annotations.Groups)
                    {
                        int count = annotations.Entries(group).Count();
                        Gui.PropertyLabel(group, $"{count} propert{(count == 1 ? "y" : "ies")}");
                    }
                }
                Gui.PropertyLabel("Textures", driver.Draft.Textures.Count.ToString());
            }
            Gui.Space(2.0f);
            if (Gui.Button("Open in material editor"))
                MaterialEditorPanel.Open(driver.Path);
        }
    }
}
