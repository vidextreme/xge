// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg;
using xg.editor;
using xg.imui;
using xg.math;

namespace xgEditor.CoreCLR
{
    // The Composition Object Editor panel (Center slot, next to Scene and Game): hosts one ObjectEditorView for one .xco
    // at a time. Open(path) opens and focuses it; switching to another object while the current one has unsaved changes
    // asks to save, discard or cancel.
    [PanelTool("Object Editor", Icon = "Object", Unique = true, Priority = 4, Tooltip = "Edit .xco composition objects")]
    public sealed class ObjectEditorPanel : Panel, IPanelTool
    {
        private const string SwitchModalId = "object_switch";

        private readonly ObjectEditorView _view = new("object_panel");
        private string? _pendingPath;
        private bool _openSwitch;

        public ObjectEditorPanel() => _view.SavedAs += path => Debug.Log($"[Objects] Editing {path}");

        public override string Title => "Object Editor";
        public override string? MenuPath => "General/Object Editor";
        public override DockSlot Slot => DockSlot.Center;
        public override Icon PanelIcon => Icon.Object;
        public override PanelFlags Flags => PanelFlags.NoPadding | PanelFlags.NoScrollbar;

        public int InstanceIndex { get; set; }
        public ObjectEditorView View => _view;
        public ObjectDriver? Driver => _view.Driver;

        public ToolUsability OnSelectItems(object[] selection) => ToolUsability.Usable;

        // Opens a composition object ("Assets/....xco") in the Object Editor panel and focuses it. A panel that wasn't
        // open docks into the Center slot, even when it was last left floating.
        public static ObjectEditorPanel Open(string path)
        {
            bool wasOpen = ImUI.RegisteredPanels.OfType<ObjectEditorPanel>().Any(p => p.IsOpen);
            ObjectEditorPanel panel = Open<ObjectEditorPanel>();
            if (!wasOpen)
                panel.Dock();
            panel.Edit(path);
            panel.Focus();
            return panel;
        }

        public void Edit(string path)
        {
            if (Driver is { } current && string.Equals(current.Path, path, StringComparison.OrdinalIgnoreCase))
                return;
            if (Driver is { HasPendingChanges: true })
            {
                _pendingPath = path;
                _openSwitch = true;
                return;
            }
            Switch(path);
        }

        private void Switch(string path)
        {
            Driver?.Dispose();
            _view.Driver = new ObjectDriver(path);
            _view.Selected = null;
            Debug.Log($"[Objects] Editing {path}");
        }

        protected override void OnGUI()
        {
            if (Driver is { } driver && ProjectService.Assets is { } assets && assets.GuidOf(driver.Path) is null && !File.Exists(assets.FullPath(driver.Path)))
                _view.Driver = null;
            _view.OnGUI();
            DrawSwitchPrompt();
        }

        private void DrawSwitchPrompt()
        {
            if (_openSwitch)
            {
                _openSwitch = false;
                Gui.OpenPopup(SwitchModalId);
            }
            using var modal = Gui.Modal(SwitchModalId, "Unsaved object", new Vector2(340.0f, 0.0f));
            if (!modal.IsOpen || _pendingPath is not string pending)
                return;
            Gui.Label($"{Driver?.Name} has unsaved changes.");
            using (Gui.Horizontal())
            {
                Gui.FlexibleSpace();
                if (Gui.Button("Cancel"))
                {
                    _pendingPath = null;
                    Gui.CloseModal();
                }
                if (Gui.Button("Discard"))
                {
                    Switch(pending);
                    _pendingPath = null;
                    Gui.CloseModal();
                }
                if (Gui.Button("Save"))
                {
                    Driver?.Apply();
                    Switch(pending);
                    _pendingPath = null;
                    Gui.CloseModal();
                }
            }
        }

        protected override void OnDestroy()
        {
            Driver?.Dispose();
            _view.Dispose();
        }
    }

    // A summary of the selected .xco (Objects, meshes, exposed ones) with a button that opens it in the Object Editor.
    [InspectorTool("Object", Icon = "Object", Priority = 0, Tooltip = "The selected composition object")]
    public sealed class ObjectInspectorTool : IInspectorTool
    {
        public ToolUsability OnSelectItems(object[] selection) =>
            selection.LastOrDefault() is AssetRef asset && ObjectAsset.IsObject(asset.Path) ? ToolUsability.Usable : ToolUsability.NotApplicable;

        public object? DriverKey(ToolSelection selection) => selection.ActiveObject as AssetRef?;

        public IToolDriver? CreateDriver(ToolSelection selection) =>
            selection.ActiveObject is AssetRef asset ? new ObjectDriver(asset.Path) : null;

        public void OnInspectorGUI(InspectorToolContext context)
        {
            ObjectDriver? driver = context.Driver as ObjectDriver;
            using var area = Gui.ScrollArea("##object_inspector", default, ScrollAreaFlags.Padded);
            if (!area.IsOpen)
                return;
            Vector2 avail = Gui.AvailableSize;
            Gui.Draw.RectFilled(new Rect(new Vector2(-6.0f, -6.0f), new Vector2(avail.x + 6.0f, 100000.0f)), Style.Color(StyleColor.ToolbarBg));
            using (Gui.Horizontal())
            {
                using (Gui.Font(FontStyle.Bold))
                    Gui.Label(driver?.Name ?? "Object");
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
            List<ObjectNodeAsset> objects = driver.Draft.Objects;
            using (Gui.Card("##object_summary", "Object", Icon.Object, null, Style.Color(StyleColor.IconWarning)))
            {
                Gui.PropertyLabel("Objects", objects.Count.ToString());
                Gui.PropertyLabel("Meshes", objects.Count(o => o.Kind == ObjectNodeKind.Mesh).ToString());
                Gui.PropertyLabel("Parts", objects.Sum(o => o.Parts.Count).ToString());
                Gui.PropertyLabel("Actors", objects.Count(o => o.Expose == ObjectNodeExpose.Actor).ToString());
                Gui.PropertyLabel("Proxies", objects.Count(o => o.Expose == ObjectNodeExpose.Proxy).ToString());
            }
            Gui.Space(2.0f);
            if (Gui.Button("Open in object editor"))
                ObjectEditorPanel.Open(driver.Path);
        }
    }
}
