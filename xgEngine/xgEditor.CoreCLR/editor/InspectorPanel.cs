// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg;
using xg.editor;
using xg.imui;
using xg.math;

namespace xgEditor.CoreCLR
{
    // Editor Inspector: a view over the Inspector tools. The tools are single instances held by Tools; this panel only
    // keeps view-level things: its selection (a retained ToolSelection holding the tools' drivers, which the lock
    // freezes), its view state for the tools, and which tool it shows (the usable one with the lowest priority: the
    // actor's components, an asset's import settings, or a project's own [InspectorTool]). Any number of Inspectors can
    // be open; unlocked ones share the editor selection's drivers.
    [PanelTool("Inspector", Icon = "Inspector", Priority = 0,
        Tooltip = "Details of the selection. Open more to compare, and lock one to keep its selection")]
    public sealed class InspectorPanel : Panel, IPanelTool
    {
        private const string PendingModalId = "inspector_pending";

        private readonly ToolView _view = new();
        private ToolSelection? _selection;
        private ToolSelection? _pendingSelection;
        private IInspectorTool? _current;
        private bool _openPending;
        private bool _pendingOpened;
        private int _toolsVersion = -1;
        private bool _locked;
        private bool _lockWhenShowing;

        public int InstanceIndex { get; set; }
        public override string Title => PanelTools.Title("Inspector", InstanceIndex);
        public override string? MenuPath => InstanceIndex == 0 ? "General/Inspector" : null;
        public override DockSlot Slot => InstanceIndex == 0 ? DockSlot.Right : DockSlot.Floating;
        public override Rect? InitialRect =>
            InstanceIndex == 0 ? null : Rect.FromXYWH(380.0f + InstanceIndex * 28.0f, 110.0f + InstanceIndex * 28.0f, 320.0f, 420.0f);
        public override Icon PanelIcon => Icon.Inspector;
        public override PanelFlags Flags => PanelFlags.NoPadding | PanelFlags.NoScrollbar;

        // 0 = All, n = the n-th component.
        public int SelectedTab
        {
            get => ComponentsView.Tab;
            set => ComponentsView.SelectTab(value);
        }

        // Locked: keep showing the current selection and ignore selection changes. Unlocking takes the current selection.
        public bool Locked
        {
            get => _locked;
            set
            {
                if (_locked == value)
                    return;
                _locked = value;
                if (!value)
                    Follow(Selection.Current);
            }
        }

        // This Inspector's selection (the editor selection, or what it held when it was locked).
        public IReadOnlyList<object> InspectedObjects => _selection?.Objects ?? Array.Empty<object>();
        public IInspectorTool? CurrentTool => _current;

        private ComponentsViewState ComponentsView => _view.Scope(typeof(ComponentsInspectorTool)).State<ComponentsViewState>();

        public ToolUsability OnSelectItems(object[] selection)
        {
            if (!_locked)
                Follow(Selection.Current);
            return ToolUsability.Usable;
        }

        // Locks and focuses the Inspector as soon as it shows an actor or another tool's content (test hook).
        public void LockWhenShowing()
        {
            _lockWhenShowing = true;
            ComponentsView.LockWhenShowing = true;
        }

        public void OpenAddComponent(int delayFrames = 0) => ComponentsView.OpenAddDelay = Math.Max(delayFrames, 0);
        public void AddComponentWhenSelected(string fullName) => ComponentsView.PendingAddType = fullName;

        protected override void OnGUI()
        {
            if (_selection is null)
                Adopt(Selection.Current);
            SyncTools();
            if (_current is null || _selection is null)
            {
                Gui.Label("No Inspector tools are registered.");
                return;
            }
            if (_lockWhenShowing && _current is not ComponentsInspectorTool)
            {
                _lockWhenShowing = false;
                Locked = true;
                Focus();
            }
            IInspectorTool tool = _current;
            _current.OnInspectorGUI(new InspectorToolContext(_selection, _selection.DriverFor(tool), _view.Scope(tool.GetType()), _locked, DrawLock, () =>
            {
                Locked = true;
                Focus();
            }));
            DrawPendingPrompt();
        }

        protected override void OnDestroy()
        {
            _pendingSelection?.Release();
            _pendingSelection = null;
            _selection?.Release();
            _selection = null;
            _view.Dispose();
        }

        private void SyncTools()
        {
            Tools? tools = Subsystem.Get<Tools>();
            if (tools is null || tools.Version == _toolsVersion)
                return;
            _toolsVersion = tools.Version;
            ChooseTool();
        }

        // Shows the next selection, unless that would deselect an item whose driver has unapplied changes: then this
        // Inspector keeps its selection (and those drivers) and asks first.
        private void Follow(ToolSelection next)
        {
            if (ReferenceEquals(next, _selection))
                return;
            if (_selection is not null && _selection.PendingOnDeselect(next.Objects).Count > 0)
            {
                _pendingSelection?.Release();
                _pendingSelection = next.Retain();
                _openPending = true;
                return;
            }
            Adopt(next);
        }

        private void Adopt(ToolSelection next)
        {
            ToolSelection? previous = _selection;
            _selection = next.Retain();
            previous?.Release();
            ChooseTool();
        }

        private void ChooseTool()
        {
            if (_selection is null)
                return;
            IInspectorTool? chosen = null;
            foreach (IInspectorTool tool in Subsystem.Get<Tools>()?.InspectorTools ?? Array.Empty<IInspectorTool>())
            {
                if (_selection.Usability(tool) == ToolUsability.Usable)
                {
                    chosen = tool;
                    break;
                }
            }
            if (ReferenceEquals(chosen, _current))
                return;
            _current = chosen;
            if (!string.IsNullOrEmpty(Environment.GetEnvironmentVariable("XG_UI_TEST_INSPECT_ASSET")))
                Debug.Log($"[Debug] Inspector {InstanceIndex}: tool {chosen?.GetType().Name ?? "none"}, scene tool {Subsystem.Get<Tools>()?.ActiveTool?.GetType().Name ?? "none"}");
        }

        private void DrawLock()
        {
            bool locked = _locked;
            Color32 color = Style.Color(locked ? StyleColor.Accent : StyleColor.Text);
            if (Gui.IconToggle("##lock", locked ? Icon.Lock : Icon.Unlock, ref locked, color))
                Locked = locked;
            Gui.Tooltip(locked ? "Locked: keeps showing this selection. Click to follow the selection again"
                : "Lock: keep showing this selection");
        }

        private void DrawPendingPrompt()
        {
            if (_openPending)
            {
                _openPending = false;
                _pendingOpened = true;
                Gui.OpenPopup(PendingModalId);
            }
            using var modal = Gui.Modal(PendingModalId, "Unapplied changes", new Vector2(340.0f, 0.0f));
            if (!modal.IsOpen)
            {
                if (_pendingOpened)
                {
                    _pendingOpened = false;
                    _pendingSelection?.Release();
                    _pendingSelection = null;
                }
                return;
            }
            Gui.Label("Apply or revert these changes before changing the selection.");
            Gui.Space(6.0f);
            using (Gui.Horizontal())
            {
                Gui.FlexibleSpace();
                if (Gui.Button("Revert"))
                    ResolvePending(false);
                if (Gui.Button("Apply"))
                    ResolvePending(true);
            }
        }

        private void ResolvePending(bool apply)
        {
            Gui.ClosePopup();
            _pendingOpened = false;
            ToolSelection? next = _pendingSelection;
            _pendingSelection = null;
            if (next is null || _selection is null)
                return;
            foreach (IToolDriver driver in _selection.PendingOnDeselect(next.Objects))
            {
                if (apply)
                    driver.Apply();
                else
                    driver.Revert();
            }
            if (!_locked && _selection.PendingOnDeselect(next.Objects).Count == 0)
                Adopt(Selection.Current);
            next.Release();
        }
    }
}
