// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg;
using xg.editor;
using xg.imui;
using xg.math;

namespace xgEditor.CoreCLR
{
    // The Material Editor panel (Center slot, next to Scene and Game): hosts one MaterialEditorView for one material at
    // a time. Open(path) opens and focuses it; switching to another material while the current one has unsaved changes
    // asks to save, discard or cancel.
    [PanelTool("Material Editor", Icon = "Material", Unique = true, Priority = 3, Tooltip = "Edit .xcm materials")]
    public sealed class MaterialEditorPanel : Panel, IPanelTool
    {
        private const string SwitchModalId = "material_switch";

        private readonly MaterialEditorView _view = new("material_panel");
        private string? _pendingPath;
        private bool _openSwitch;

        public override string Title => "Material Editor";
        public override string? MenuPath => "General/Material Editor";
        public override DockSlot Slot => DockSlot.Center;
        public override Icon PanelIcon => Icon.Material;
        public override PanelFlags Flags => PanelFlags.NoPadding | PanelFlags.NoScrollbar;

        public int InstanceIndex { get; set; }
        public MaterialEditorView View => _view;
        public MaterialDriver? Driver => _view.Driver;

        public ToolUsability OnSelectItems(object[] selection) => ToolUsability.Usable;

        // Opens a material asset ("Assets/....xcm") in the Material Editor panel and focuses it. A panel that wasn't open
        // docks into the Center slot (next to Scene and Game), even when it was last left floating.
        public static MaterialEditorPanel Open(string path)
        {
            bool wasOpen = ImUI.RegisteredPanels.OfType<MaterialEditorPanel>().Any(p => p.IsOpen);
            MaterialEditorPanel panel = Open<MaterialEditorPanel>();
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
            _view.Driver = new MaterialDriver(path);
            Debug.Log($"[Materials] Editing {path}");
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
            using var modal = Gui.Modal(SwitchModalId, "Unsaved material", new Vector2(340.0f, 0.0f));
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
}
