// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Reflection;
using xg.imui;
using xg.math;

namespace xg.editor
{
    // The editor's tool system, a managed subsystem (Subsystem.Get<Tools>()): registers [EditorTool] classes and
    // [ToolButton]/[ToolToggle] methods from the editor's assemblies and from a project's editor scripts. Selection calls
    // NotifySelection on every change; Tools then asks every tool (and every control validator) what it can do with the
    // selection. Many tools can be usable, but only one is active: the user picks it in the Tools ribbon, and the active
    // tool receives the tool events and drives the editor's behaviour (its GUI, later its gizmo and pointer handling).
    // When the active tool stops being usable, the default tool ([EditorTool(ActiveByDefault = true)]) takes over.
    // Tools that don't implement IActivatableTool are passive (selection only, no ribbon item). Panel tools ([PanelTool]
    // IPanelTool panels) are passive tools listed under View > Tool Windows, where the ribbon opens or toggles them.
    // Every [EditorTool] and [InspectorTool] has exactly one instance, created here. Tools are presentation: their data
    // lives in drivers (IToolDriver) that the selection (ToolSelection, Selection.Current or a locked Inspector's) creates
    // and disposes, never here.
    public sealed class Tools : ISubsystem
    {
        private sealed class PanelTypeEntry
        {
            public required Type Type;
            public required PanelToolAttribute Attribute;
            public required int Order;
            public ComponentIcon Icon;
        }

        private sealed class ToolEntry
        {
            public required ITool Tool;
            public required EditorToolAttribute Attribute;
            public required int Order;
            public ToolGUIAttribute? GuiAttribute;
            public ToolUsability Usability = ToolUsability.Usable;
            public bool Active;
            public ComponentIcon Icon;
        }

        private sealed class ControlEntry
        {
            public required MethodInfo Method;
            public required object? Target;
            public required string Label;
            public required string Key;
            public required bool IsToggle;
            public required ToolLayoutAttribute Layout;
            public required string Category;
            public required string Group;
            public required int Order;
            public MethodInfo? Validator;
            public ToolUsability Usability = ToolUsability.Usable;
            public bool State;
            public ComponentIcon Icon;
        }

        private readonly struct Entry
        {
            public Entry(string category, string group, int priority, int order, RibbonItem item)
            {
                Category = category;
                Group = group;
                Priority = priority;
                Order = order;
                Item = item;
            }

            public string Category { get; }
            public string Group { get; }
            public int Priority { get; }
            public int Order { get; }
            public RibbonItem Item { get; }
        }

        private const string DefaultCategory = "Tools";
        private const string DefaultGroup = "General";

        private readonly List<ToolEntry> _tools = new();
        private readonly List<ControlEntry> _controls = new();
        private readonly List<PanelTypeEntry> _panelTypes = new();
        private readonly List<(IInspectorTool Tool, InspectorToolAttribute Attribute, int Order)> _inspectorTools = new();
        private readonly List<IPanelTool> _panels = new();
        private readonly List<(IContextMenuTool Tool, int Order)> _menuTools = new();
        private readonly HashSet<IPanelTool> _seenOpen = new();
        private ToolEntry? _active;
        private readonly Dictionary<string, ToolCategoryAttribute> _categories = new();
        private readonly Dictionary<(string, string), ToolGroupAttribute> _groups = new();
        private readonly HashSet<Assembly> _assemblies = new();
        private readonly HashSet<string> _reported = new();
        private object[] _selection = Array.Empty<object>();
        private int _order;

        // Replace with the undo system once it exists; every state change made by tools goes through it.
        public IUndoRecorder Undo { get; set; } = NullUndoRecorder.Instance;

        // Replace with a persistent store to keep toggle states between sessions.
        public IToolToggleStore ToggleStore { get; set; } = new MemoryToolToggleStore();

        // Increments whenever tools or controls are added or removed; hosts rebuild their ribbon when it changes.
        public int Version { get; private set; }

        public object[] CurrentSelection => _selection;
        public int ToolCount => _tools.Count;
        public int ControlCount => _controls.Count;
        // The one active tool, or null.
        public ITool? ActiveTool => _active?.Tool;

        public T? GetTool<T>() where T : class, ITool => _tools.Select(e => e.Tool).OfType<T>().FirstOrDefault();

        // Registered instances of panel tools (any type), e.g. every open or hidden Inspector.
        public IReadOnlyList<IPanelTool> PanelInstances => _panels;

        // The Inspector tools (one instance each), the ones tried first come first. Every Inspector draws these same
        // instances with its own selection, drivers and view state.
        public IReadOnlyList<IInspectorTool> InspectorTools { get; private set; } = Array.Empty<IInspectorTool>();

        // Every context menu tool: the tools above and the open panel tools that implement IContextMenuTool, plus one
        // instance of each other concrete IContextMenuTool class, lowest ContextMenuPriority first.
        public IReadOnlyList<IContextMenuTool> ContextMenuTools
        {
            get
            {
                List<(IContextMenuTool Tool, int Order)> all = new();
                foreach (ToolEntry entry in _tools)
                    if (entry.Tool is IContextMenuTool tool)
                        all.Add((tool, entry.Order));
                foreach (var inspector in _inspectorTools)
                    if (inspector.Tool is IContextMenuTool tool)
                        all.Add((tool, inspector.Order));
                for (int i = 0; i < _panels.Count; i++)
                    if (_panels[i] is IContextMenuTool tool)
                        all.Add((tool, int.MaxValue / 2 + i));
                all.AddRange(_menuTools);
                return all.OrderBy(t => t.Tool.ContextMenuPriority).ThenBy(t => t.Order).Select(t => t.Tool).Distinct().ToArray();
            }
        }

        public Tools()
        {
            _categories[PanelTools.Category] = new ToolCategoryAttribute(PanelTools.Category)
            {
                Icon = "View",
                Priority = 1000,
                Tooltip = "Tool windows: open, show or hide editor panels",
            };
            _groups[(PanelTools.Category, PanelTools.Group)] = new ToolGroupAttribute(PanelTools.Category, PanelTools.Group)
            {
                Width = 170.0f,
                Tooltip = "Toggles show or hide a window; buttons open another instance",
            };
        }

        // ------------------------------------------------------------
        // Subsystem
        // ------------------------------------------------------------
        public void Initialize()
        {
            int tools = _tools.Count;
            int controls = _controls.Count;
            foreach (Assembly assembly in TypeBridge.EditorContext.Assemblies.ToArray())
                RegisterAssembly(assembly, false);
            Debug.Log($"[Tools] registered {_tools.Count - tools} tool(s), {_controls.Count - controls} control(s)");
            ActivateDefault();
            SyncSelection();
        }

        public void Update(float dt) => Tick();

        public void Shutdown()
        {
            SetActive(null);
            foreach (ToolEntry entry in _tools)
                if (entry.Tool is IDisposable disposable)
                    Safe(entry.Attribute.Name, disposable.Dispose);
            foreach (var inspectorTool in _inspectorTools)
                if (inspectorTool.Tool is IDisposable disposable)
                    Safe(inspectorTool.Attribute.Name, disposable.Dispose);
            _tools.Clear();
            _controls.Clear();
            _panelTypes.Clear();
            _inspectorTools.Clear();
            InspectorTools = Array.Empty<IInspectorTool>();
            _panels.Clear();
            _seenOpen.Clear();
            _categories.Clear();
            _groups.Clear();
            _assemblies.Clear();
            Version++;
        }

        // Per-frame upkeep (called by the Tools panel): drops selection items that are no longer valid, picks up new
        // panel-tool instances (and gives them the selection), forgets unregistered ones, and unregisters closed extra
        // instances (here, never inside their own close callback).
        public void Tick()
        {
            Selection.Validate();
            SyncPanels();
        }

        private void SyncPanels()
        {
            IReadOnlyList<Panel> registered = ImUI.RegisteredPanels;
            foreach (Panel panel in registered.ToArray())
            {
                if (panel is not IPanelTool tool)
                    continue;
                if (!_panels.Contains(tool))
                {
                    _panels.Add(tool);
                    Safe(panel.GetType().Name, () => tool.OnSelectItems(_selection));
                }
                if (panel.IsOpen)
                    _seenOpen.Add(tool);
                else if (tool.InstanceIndex > 0 && _seenOpen.Contains(tool))
                    panel.Unregister();
            }
            for (int i = _panels.Count - 1; i >= 0; --i)
            {
                if (_panels[i] is not Panel panel || !registered.Contains(panel))
                {
                    _seenOpen.Remove(_panels[i]);
                    _panels.RemoveAt(i);
                }
            }
        }

        // ------------------------------------------------------------
        // Registration
        // ------------------------------------------------------------
        public void RegisterAssembly(Assembly assembly) => RegisterAssembly(assembly, true);

        private void RegisterAssembly(Assembly assembly, bool log)
        {
            if (assembly.IsDynamic || !_assemblies.Add(assembly) || !UsesToolApi(assembly))
                return;
            int tools = _tools.Count;
            int controls = _controls.Count;

            foreach (ToolCategoryAttribute category in assembly.GetCustomAttributes<ToolCategoryAttribute>())
                _categories[category.Name] = category;
            foreach (ToolGroupAttribute group in assembly.GetCustomAttributes<ToolGroupAttribute>())
                _groups[(group.Category, group.Name)] = group;

            Type[] types;
            try
            {
                types = assembly.GetTypes();
            }
            catch (ReflectionTypeLoadException e)
            {
                types = e.Types.Where(t => t is not null).ToArray()!;
            }
            int panelTypes = _panelTypes.Count;
            int inspectorTools = _inspectorTools.Count;
            int menuTools = _menuTools.Count;
            foreach (Type type in types.OrderBy(t => t.MetadataToken))
            {
                ToolEntry? tool = RegisterTool(type);
                RegisterControls(type, tool);
                RegisterPanelType(type);
                RegisterInspectorTool(type);
                RegisterContextMenuTool(type);
            }

            if (_tools.Count != tools || _controls.Count != controls || _panelTypes.Count != panelTypes || _inspectorTools.Count != inspectorTools
                || _menuTools.Count != menuTools)
            {
                Version++;
                if (log)
                {
                    Debug.Log($"[Tools] {assembly.GetName().Name}: registered {_tools.Count - tools} tool(s), {_controls.Count - controls} control(s)");
                    ActivateDefault();
                    SyncSelection();
                }
                Refresh();
            }
        }

        // Removes everything assembly registered (a project's editor scripts before a reload): its tools, controls,
        // panel tools (closing their panels) and inspector tools.
        public void UnregisterAssembly(Assembly assembly)
        {
            if (!_assemblies.Remove(assembly))
                return;
            bool Owned(Type? type) => type is not null && type.Assembly == assembly;

            if (_active is not null && Owned(_active.Tool.GetType()))
                SetActive(null);
            foreach (ToolEntry entry in _tools.Where(e => Owned(e.Tool.GetType())).ToArray())
            {
                if (entry.Tool is IDisposable disposable)
                    Safe(entry.Attribute.Name, disposable.Dispose);
                _tools.Remove(entry);
            }
            _controls.RemoveAll(c => Owned(c.Method.DeclaringType));
            _panelTypes.RemoveAll(e => Owned(e.Type));
            foreach (var inspectorTool in _inspectorTools.Where(t => Owned(t.Tool.GetType())).ToArray())
            {
                if (inspectorTool.Tool is IDisposable disposable)
                    Safe(inspectorTool.Attribute.Name, disposable.Dispose);
                _inspectorTools.Remove(inspectorTool);
            }
            InspectorTools = _inspectorTools.OrderBy(t => t.Attribute.Priority).ThenBy(t => t.Order).Select(t => t.Tool).ToArray();
            foreach (var menuTool in _menuTools.Where(t => Owned(t.Tool.GetType())).ToArray())
            {
                if (menuTool.Tool is IDisposable disposable)
                    Safe(menuTool.Tool.GetType().Name, disposable.Dispose);
                _menuTools.Remove(menuTool);
            }
            foreach (IPanelTool panel in _panels.Where(p => Owned(p.GetType())).ToArray())
            {
                _panels.Remove(panel);
                _seenOpen.Remove(panel);
            }
            Version++;
            ActivateDefault();
            Refresh();
        }

        // Delivers a selection made before these tools were registered.
        private void SyncSelection()
        {
            if (Selection.Count > 0 && !ReferenceEquals(_selection, Selection.Objects))
                NotifySelection(Selection.Current);
        }

        private static bool UsesToolApi(Assembly assembly)
        {
            Assembly api = typeof(Tools).Assembly;
            if (assembly == api)
                return true;
            string? name = api.GetName().Name;
            return assembly.GetReferencedAssemblies().Any(r => r.Name == name);
        }

        private ToolEntry? RegisterTool(Type type)
        {
            EditorToolAttribute? attribute = type.GetCustomAttribute<EditorToolAttribute>();
            if (attribute is null)
                return null;
            if (type.IsAbstract || !typeof(ITool).IsAssignableFrom(type) || type.GetConstructor(Type.EmptyTypes) is null)
            {
                Debug.LogError($"[Tools] {type.FullName}: [EditorTool] needs a non-abstract ITool class with a parameterless constructor");
                return null;
            }
            ITool tool;
            try
            {
                tool = (ITool)Activator.CreateInstance(type)!;
            }
            catch (Exception e)
            {
                Debug.LogError($"[Tools] {type.FullName}: constructor threw: {(e as TargetInvocationException)?.InnerException ?? e}");
                return null;
            }
            ToolEntry entry = new()
            {
                Tool = tool,
                Attribute = attribute,
                Order = _order++,
                GuiAttribute = type.GetCustomAttribute<ToolGUIAttribute>(),
                Icon = ComponentIcons.Resolve(attribute.Icon, attribute.IconColor, imui.Icon.Gizmo),
            };
            entry.Usability = SafeUsability(attribute.Name, () => tool.OnSelectItems(_selection));
            if (attribute.ActiveByDefault && tool is not IActivatableTool)
                Debug.LogWarning($"[Tools] {type.FullName}: ActiveByDefault is ignored on a passive tool (it doesn't implement IActivatableTool)");
            else if (attribute.ActiveByDefault && DefaultTool is ToolEntry existing)
                Debug.LogWarning($"[Tools] {type.FullName}: '{existing.Attribute.Name}' is already the default tool; only one ActiveByDefault tool is used");
            _tools.Add(entry);
            return entry;
        }

        private void RegisterInspectorTool(Type type)
        {
            InspectorToolAttribute? attribute = type.GetCustomAttribute<InspectorToolAttribute>();
            if (attribute is null)
                return;
            if (type.IsAbstract || !typeof(IInspectorTool).IsAssignableFrom(type) || type.GetConstructor(Type.EmptyTypes) is null)
            {
                Debug.LogError($"[Tools] {type.FullName}: [InspectorTool] needs a non-abstract IInspectorTool with a parameterless constructor");
                return;
            }
            IInspectorTool tool;
            try
            {
                tool = (IInspectorTool)Activator.CreateInstance(type)!;
            }
            catch (Exception e)
            {
                Debug.LogError($"[Tools] {type.FullName}: constructor threw: {(e as TargetInvocationException)?.InnerException ?? e}");
                return;
            }
            _inspectorTools.Add((tool, attribute, _order++));
            InspectorTools = _inspectorTools.OrderBy(t => t.Attribute.Priority).ThenBy(t => t.Order).Select(t => t.Tool).ToArray();
        }

        // A concrete IContextMenuTool that no other registration holds (not an [EditorTool], [InspectorTool] or panel).
        private void RegisterContextMenuTool(Type type)
        {
            if (type.IsAbstract || type.IsInterface || !typeof(IContextMenuTool).IsAssignableFrom(type) || typeof(Panel).IsAssignableFrom(type)
                || type.IsDefined(typeof(EditorToolAttribute), false) || type.IsDefined(typeof(InspectorToolAttribute), false))
                return;
            if (type.GetConstructor(Type.EmptyTypes) is null)
            {
                Debug.LogError($"[Tools] {type.FullName}: an IContextMenuTool needs a parameterless constructor");
                return;
            }
            try
            {
                _menuTools.Add(((IContextMenuTool)Activator.CreateInstance(type)!, _order++));
            }
            catch (Exception e)
            {
                Debug.LogError($"[Tools] {type.FullName}: constructor threw: {(e as TargetInvocationException)?.InnerException ?? e}");
            }
        }

        internal ToolUsability SafeMenuUsability(IContextMenuTool tool, ContextMenuContext context) =>
            SafeUsability(tool.GetType().Name, () => tool.OnContextMenu(context));

        internal void SafeMenuGUI(IContextMenuTool tool, ContextMenu menu, ContextMenuContext context) =>
            Safe(tool.GetType().Name, () => tool.OnContextMenuGUI(menu, context));

        private void RegisterPanelType(Type type)
        {
            PanelToolAttribute? attribute = type.GetCustomAttribute<PanelToolAttribute>();
            if (attribute is null)
                return;
            if (type.IsAbstract || !typeof(Panel).IsAssignableFrom(type) || !typeof(IPanelTool).IsAssignableFrom(type)
                || type.GetConstructor(Type.EmptyTypes) is null)
            {
                Debug.LogError($"[Tools] {type.FullName}: [PanelTool] needs a non-abstract Panel that implements IPanelTool and has a parameterless constructor");
                return;
            }
            _panelTypes.Add(new PanelTypeEntry
            {
                Type = type,
                Attribute = attribute,
                Order = _order++,
                Icon = ComponentIcons.Resolve(attribute.Icon, attribute.IconColor, imui.Icon.Inspector),
            });
        }

        // ------------------------------------------------------------
        // Panel tools
        // ------------------------------------------------------------
        public T? OpenPanel<T>() where T : Panel, IPanelTool => OpenPanel(typeof(T)) as T;

        // The ribbon action of a panel tool. Unique: show the instance (creating it if needed) or hide it when it's
        // open. Non-unique: reopen the first instance when it's closed, otherwise open another instance. Returns the
        // panel that was shown, or null when it was hidden.
        public Panel? OpenPanel(Type type)
        {
            PanelTypeEntry? entry = _panelTypes.FirstOrDefault(e => e.Type == type);
            List<Panel> instances = Instances(type);
            Panel? first = instances.FirstOrDefault(p => ((IPanelTool)p).InstanceIndex == 0) ?? instances.FirstOrDefault();
            bool unique = entry?.Attribute.Unique ?? false;
            if (first is not null && (unique || !first.IsOpen))
            {
                if (unique && first.IsOpen)
                {
                    first.Close();
                    return null;
                }
                first.Open();
                first.Focus();
                return first;
            }
            int index = 0;
            while (instances.Any(p => ((IPanelTool)p).InstanceIndex == index))
                ++index;
            Panel? panel = null;
            Safe(type.Name, () =>
            {
                Panel created = (Panel)Activator.CreateInstance(type)!;
                ((IPanelTool)created).InstanceIndex = index;
                if (created.Register())
                {
                    created.Open();
                    panel = created;
                }
            });
            SyncPanels();
            return panel;
        }

        private static List<Panel> Instances(Type type) => ImUI.RegisteredPanels.Where(p => p.GetType() == type).ToList();

        private RibbonItem PanelItem(PanelTypeEntry entry)
        {
            PanelToolAttribute a = entry.Attribute;
            Type type = entry.Type;
            RibbonItem item = new()
            {
                Label = a.Name,
                Tooltip = a.Tooltip,
                Style = RibbonItemStyle.Wide,
                IsToggled = a.Unique ? () => Instances(type).Any(p => p.IsOpen) : null,
                Dot = a.Unique ? ToggleDot.Check : ToggleDot.None,
                Clicked = () => OpenPanel(type),
            };
            ApplyIcon(item, entry.Icon, a.IconColor);
            return item;
        }

        private void RegisterControls(Type type, ToolEntry? tool)
        {
            const BindingFlags flags = BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Static | BindingFlags.Instance | BindingFlags.DeclaredOnly;
            foreach (MethodInfo method in type.GetMethods(flags).OrderBy(m => m.MetadataToken))
            {
                ToolButtonAttribute? button = method.GetCustomAttribute<ToolButtonAttribute>();
                ToolToggleAttribute? toggle = method.GetCustomAttribute<ToolToggleAttribute>();
                if (button is null && toggle is null)
                    continue;
                string where = $"{type.FullName}.{method.Name}";
                ToolLayoutAttribute layout = (ToolLayoutAttribute?)button ?? toggle!;
                string label = button?.Label ?? toggle!.Label;
                bool isToggle = button is null;

                bool signatureOk = isToggle
                    ? method.ReturnType == typeof(bool) && method.GetParameters() is [{ ParameterType: var p }] && p == typeof(bool)
                    : method.ReturnType == typeof(void) && method.GetParameters().Length == 0;
                if (!signatureOk)
                {
                    Debug.LogError($"[Tools] {where}: " + (isToggle ? "[ToolToggle] needs bool M(bool requested)" : "[ToolButton] needs void M()"));
                    continue;
                }
                object? target = null;
                if (!method.IsStatic)
                {
                    if (tool is null)
                    {
                        Debug.LogError($"[Tools] {where}: instance controls must be declared on an [EditorTool] class; make it static");
                        continue;
                    }
                    target = tool.Tool;
                }

                string? category = layout.Category ?? tool?.Attribute.Category;
                string? group = layout.Group ?? tool?.Attribute.Group;
                if (method.IsStatic && tool is null && (category is null || group is null))
                {
                    Debug.LogError($"[Tools] {where}: Category and Group are required on a control outside a tool class");
                    continue;
                }

                MethodInfo? validator = null;
                string? validate = button?.Validate ?? toggle?.Validate;
                if (validate is not null)
                {
                    validator = type.GetMethod(validate, flags);
                    if (validator is null || validator.GetCustomAttribute<ToolValidatorAttribute>() is null
                        || validator.ReturnType != typeof(ToolUsability) || validator.GetParameters().Length != 0
                        || (!validator.IsStatic && target is null))
                    {
                        Debug.LogError($"[Tools] {where}: Validate = \"{validate}\" must name a [ToolValidator] ToolUsability {validate}() on {type.Name}");
                        validator = null;
                    }
                }

                string key = $"{type.FullName}.{method.Name}";
                ControlEntry entry = new()
                {
                    Method = method,
                    Target = target,
                    Label = label,
                    Key = key,
                    IsToggle = isToggle,
                    Layout = layout,
                    Category = category ?? DefaultCategory,
                    Group = group ?? DefaultGroup,
                    Order = _order++,
                    Validator = validator,
                    Icon = ComponentIcons.Resolve(layout.Icon, layout.IconColor, imui.Icon.None),
                };
                if (isToggle)
                    entry.State = ToggleStore.TryGet(key, out bool stored) ? stored : toggle!.Default;
                _controls.Add(entry);
            }
        }

        // The activatable ActiveByDefault tool with the lowest priority (then registration order).
        private ToolEntry? DefaultTool =>
            _tools.Where(e => e.Attribute.ActiveByDefault && e.Tool is IActivatableTool)
                .OrderBy(e => e.Attribute.Priority).ThenBy(e => e.Order).FirstOrDefault();

        private void ActivateDefault()
        {
            if (_active is null && DefaultTool is ToolEntry fallback && IsUsable(fallback))
                SetActive(fallback);
        }

        // ------------------------------------------------------------
        // Activation: one active tool at a time
        // ------------------------------------------------------------
        public bool IsActive(ITool tool) => _active is not null && ReferenceEquals(_active.Tool, tool);

        // Makes the tool the active one (replacing the current one) if it is usable; activating the active tool again
        // keeps it active.
        public void Activate(ITool tool)
        {
            if (Find(tool) is ToolEntry entry && entry != _active && IsUsable(entry))
                SetActive(entry);
        }

        // Deactivates the active tool; the default tool takes over when it's usable.
        public void Deactivate()
        {
            ToolEntry? leaving = _active;
            if (leaving is null)
                return;
            SetActive(null);
            FallBack(leaving);
        }

        private ToolEntry? Find(ITool tool) => _tools.FirstOrDefault(e => ReferenceEquals(e.Tool, tool));

        private static bool IsUsable(ToolEntry entry) => entry.Tool is IActivatableTool && entry.Usability == ToolUsability.Usable;

        private void FallBack(ToolEntry leaving)
        {
            if (DefaultTool is ToolEntry fallback && fallback != leaving && IsUsable(fallback))
                SetActive(fallback);
        }

        private void SetActive(ToolEntry? entry)
        {
            ToolEntry? previous = _active;
            if (previous == entry)
                return;
            if (previous is not null)
            {
                _active = null;
                previous.Active = false;
                if (previous.Tool is EditorTool previousTool)
                    previousTool.IsActive = false;
                if (previous.Tool is IActivatableTool leaving)
                    Safe(previous.Attribute.Name, leaving.OnDeactivate);
            }
            if (entry is null)
                return;
            _active = entry;
            entry.Active = true;
            if (entry.Tool is EditorTool editorTool)
                editorTool.IsActive = true;
            ToolContext context = new(this, entry.Tool);
            if (entry.Tool is IActivatableTool activatable)
                Safe(entry.Attribute.Name, () => activatable.OnActivate(context));
        }

        // ------------------------------------------------------------
        // Selection and validation
        // ------------------------------------------------------------
        // Called by Selection on every change: re-asks every tool, panel-tool instance and validator (the active tool hands
        // over to the default tool when it can no longer work), then delivers an asset selection to the active tool.
        public void NotifySelection(ToolSelection selection)
        {
            _selection = selection.Objects;
            Refresh();
            Debug.Log($"[Tools] selection: {selection.Count} item(s)");

            AssetRef[] assets = selection.Items<AssetRef>().ToArray();
            ToolEntry? active = _active;
            if (assets.Length == 0 || active is null)
                return;
            if (active.Tool is ISelectAssetItem single && assets.Length == 1)
                Safe(active.Attribute.Name, () => single.OnSelectAssetItem(assets[0]));
            if (active.Tool is ISelectAssetItems many)
                Safe(active.Attribute.Name, () => many.OnSelectAssetItems(assets));
        }

        // Re-runs OnSelectItems and every validator now (e.g. after state the validators read has changed).
        public void Invalidate() => Refresh();

        private void Refresh()
        {
            foreach (ToolEntry entry in _tools)
            {
                ITool tool = entry.Tool;
                entry.Usability = SafeUsability(entry.Attribute.Name, () => tool.OnSelectItems(_selection));
            }
            if (_active is ToolEntry current && !IsUsable(current))
            {
                SetActive(null);
                FallBack(current);
            }
            ActivateDefault();
            foreach (IPanelTool panel in _panels.ToArray())
                Safe(panel.GetType().Name, () => panel.OnSelectItems(_selection));
            foreach (ControlEntry control in _controls)
            {
                MethodInfo? validator = control.Validator;
                control.Usability = validator is null
                    ? ToolUsability.Usable
                    : SafeUsability(control.Key, () => (ToolUsability)validator.Invoke(validator.IsStatic ? null : control.Target, null)!);
            }
        }

        private void Invoke(ControlEntry control)
        {
            using (Undo.Group(control.Label))
            {
                if (control.IsToggle)
                {
                    bool requested = !control.State;
                    bool accepted = requested;
                    Safe(control.Key, () => accepted = (bool)control.Method.Invoke(control.Target, [requested])!);
                    control.State = accepted;
                    ToggleStore.Set(control.Key, accepted);
                }
                else
                {
                    Safe(control.Key, () => control.Method.Invoke(control.Target, null));
                }
            }
            Refresh();
        }

        public bool GetToggle(string key) => _controls.FirstOrDefault(c => c.IsToggle && c.Key == key)?.State ?? false;

        // ------------------------------------------------------------
        // Scene view events for the active tool (declared now; the scene view calls these once it's wired)
        // ------------------------------------------------------------
        public void DispatchPointer(PointerEventType type, PointerEventData data)
        {
            ToolEntry? entry = _active;
            if (entry is null || data.Used)
                return;
            ITool tool = entry.Tool;
            Safe(entry.Attribute.Name, () =>
            {
                switch (type)
                {
                    case PointerEventType.Down when tool is IPointerDown down: down.OnPointerDown(data); break;
                    case PointerEventType.Up when tool is IPointerUp up: up.OnPointerUp(data); break;
                    case PointerEventType.Click when tool is IPointerClick click: click.OnPointerClick(data); break;
                    case PointerEventType.Move when tool is IPointerMove move: move.OnPointerMove(data); break;
                    case PointerEventType.BeginDrag when tool is IBeginDrag begin: begin.OnBeginDrag(data); break;
                    case PointerEventType.Drag when tool is IDrag drag: drag.OnDrag(data); break;
                    case PointerEventType.EndDrag when tool is IEndDrag end: end.OnEndDrag(data); break;
                    case PointerEventType.Scroll when tool is IScroll scroll: scroll.OnScroll(data); break;
                }
            });
        }

        public void DrawScene(SceneViewContext view)
        {
            if (_active?.Tool is IEditorSceneDraw draw)
                Safe(_active.Attribute.Name, () => draw.OnSceneGUI(view));
        }

        // ------------------------------------------------------------
        // Ribbon
        // ------------------------------------------------------------
        // Rebuilds the ribbon's categories from the registered tools, controls and tool GUIs. Item state is read each
        // frame from the cached usability, so only call this when Version changes.
        public void BuildRibbon(Ribbon ribbon)
        {
            ribbon.Clear();
            List<Entry> entries = new();
            foreach (ToolEntry tool in _tools)
            {
                if (tool.Tool is not IActivatableTool)
                    continue;
                EditorToolAttribute a = tool.Attribute;
                string category = a.Category ?? DefaultCategory;
                string group = a.Group ?? DefaultGroup;
                entries.Add(new Entry(category, group, a.Priority, tool.Order, ToolItem(tool)));
                if (tool.Tool is IEditorGUIDraw gui)
                {
                    ToolGUIAttribute? placement = tool.GuiAttribute;
                    int priority = placement is null || placement.Priority == int.MinValue ? a.Priority + 1000 : placement.Priority;
                    entries.Add(new Entry(placement?.Category ?? category, placement?.Group ?? group, priority, tool.Order, GuiItem(tool, gui)));
                }
            }
            foreach (ControlEntry control in _controls)
                entries.Add(new Entry(control.Category, control.Group, control.Layout.Priority, control.Order, ControlItem(control)));
            foreach (PanelTypeEntry panel in _panelTypes)
                entries.Add(new Entry(PanelTools.Category, PanelTools.Group, panel.Attribute.Priority, panel.Order, PanelItem(panel)));

            List<string> categoryOrder = entries.Select(e => e.Category).Distinct().ToList();
            List<string> categoryFirstUse = categoryOrder.ToList();
            categoryOrder.Sort((x, y) =>
            {
                int byPriority = CategoryPriority(x).CompareTo(CategoryPriority(y));
                if (byPriority != 0)
                    return byPriority;
                int byDeclared = _categories.ContainsKey(y).CompareTo(_categories.ContainsKey(x));
                return byDeclared != 0 ? byDeclared : categoryFirstUse.IndexOf(x).CompareTo(categoryFirstUse.IndexOf(y));
            });
            foreach (string categoryName in categoryOrder.ToArray())
            {
                _categories.TryGetValue(categoryName, out ToolCategoryAttribute? declared);
                ComponentIcon icon = ComponentIcons.Resolve(declared?.Icon, declared?.IconColor, imui.Icon.Gizmo);
                RibbonCategory category = new()
                {
                    Name = categoryName,
                    Tooltip = declared?.Tooltip,
                    Icon = icon.Icon == imui.Icon.None ? imui.Icon.Gizmo : icon.Icon,
                    IconColor = declared?.IconColor is null ? null : icon.Color,
                };
                List<Entry> inCategory = entries.Where(e => e.Category == categoryName).ToList();
                List<string> groupOrder = inCategory.Select(e => e.Group).Distinct().ToList();
                List<string> firstUse = groupOrder.ToList();
                groupOrder.Sort((x, y) =>
                {
                    int byPriority = GroupPriority(categoryName, x).CompareTo(GroupPriority(categoryName, y));
                    if (byPriority != 0)
                        return byPriority;
                    int byDeclared = _groups.ContainsKey((categoryName, y)).CompareTo(_groups.ContainsKey((categoryName, x)));
                    return byDeclared != 0 ? byDeclared : firstUse.IndexOf(x).CompareTo(firstUse.IndexOf(y));
                });
                foreach (string groupName in groupOrder)
                {
                    _groups.TryGetValue((categoryName, groupName), out ToolGroupAttribute? g);
                    RibbonGroup group = new()
                    {
                        Name = groupName,
                        Tooltip = g?.Tooltip,
                        Collapsed = g?.Collapsed ?? false,
                        IconColumns = g?.IconColumns ?? 4,
                        Width = g?.Width ?? 150.0f,
                    };
                    foreach (Entry entry in inCategory.Where(e => e.Group == groupName).OrderBy(e => e.Priority).ThenBy(e => e.Order))
                        group.Items.Add(entry.Item);
                    category.Groups.Add(group);
                }
                ribbon.Categories.Add(category);
            }
        }

        private int CategoryPriority(string name) => _categories.TryGetValue(name, out ToolCategoryAttribute? c) ? c.Priority : 0;

        private int GroupPriority(string category, string name) =>
            _groups.TryGetValue((category, name), out ToolGroupAttribute? g) ? g.Priority : 0;

        private RibbonItem ToolItem(ToolEntry tool)
        {
            EditorToolAttribute a = tool.Attribute;
            RibbonItem item = new()
            {
                Label = a.Name,
                Tooltip = a.Tooltip is null ? null : $"{a.Name}\n{a.Tooltip}",
                Style = a.Style == ToolItemStyle.Wide ? RibbonItemStyle.Wide : RibbonItemStyle.Icon,
                IsEnabled = () => tool.Usability == ToolUsability.Usable,
                IsVisible = () => tool.Usability != ToolUsability.NotApplicable,
                IsToggled = () => tool.Active,
                Clicked = () => Activate(tool.Tool),
            };
            ApplyIcon(item, tool.Icon, a.IconColor);
            return item;
        }

        private RibbonItem ControlItem(ControlEntry control)
        {
            ToolLayoutAttribute a = control.Layout;
            RibbonItem item = new()
            {
                Label = control.Label,
                Tooltip = a.Tooltip,
                Style = a.Style == ToolItemStyle.Icon ? RibbonItemStyle.Icon : RibbonItemStyle.Wide,
                IsEnabled = () => control.Usability == ToolUsability.Usable,
                IsVisible = () => control.Usability != ToolUsability.NotApplicable,
                IsToggled = control.IsToggle ? () => control.State : null,
                Dot = control.IsToggle ? ToggleDot.Check : ToggleDot.None,
                Clicked = () => Invoke(control),
            };
            ApplyIcon(item, control.Icon, a.IconColor);
            return item;
        }

        private RibbonItem GuiItem(ToolEntry tool, IEditorGUIDraw gui) => new()
        {
            Label = tool.Attribute.Name,
            IsVisible = () => tool.Active,
            OnGUI = () => Safe(tool.Attribute.Name, gui.OnGUI),
        };

        private static void ApplyIcon(RibbonItem item, ComponentIcon icon, string? colorHex)
        {
            if (icon.Texture != null && icon.Texture.IsValid)
            {
                Texture texture = icon.Texture;
                item.DrawIcon = (draw, rect, _) => draw.Image(texture, rect);
                return;
            }
            item.Icon = icon.Icon;
            item.IconColor = colorHex is null ? null : icon.Color;
        }

        // ------------------------------------------------------------
        // Error handling: tool code never breaks the editor; each failure is reported once per tool or control.
        // ------------------------------------------------------------
        private void Safe(string owner, Action action)
        {
            try
            {
                action();
            }
            catch (Exception e)
            {
                Report(owner, e);
            }
        }

        private ToolUsability SafeUsability(string owner, Func<ToolUsability> query)
        {
            try
            {
                return query();
            }
            catch (Exception e)
            {
                Report(owner, e);
                return ToolUsability.Unusable;
            }
        }

        private void Report(string owner, Exception e)
        {
            Exception inner = (e as TargetInvocationException)?.InnerException ?? e;
            if (_reported.Add(owner + "|" + inner.GetType().FullName))
                Debug.LogError($"[Tools] {owner} threw (reported once): {inner}");
        }
    }
}
