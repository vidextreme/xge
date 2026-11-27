// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg;
using xg.editor;
using xg.imui;
using xg.math;

namespace xgEditor.CoreCLR
{
    // The components of the selected actor as Ribbon-style cards, with fields drawn by PropertyDrawers (built-in ones, or
    // a project's own from its Editor scripts). A vertical tab strip on the side away from the dock edge shows All (every
    // card), one tab per component (its [ComponentIcon]) and + (Add Component). Edits apply immediately to the live
    // components, which may belong to any CoreCLR module. The actor's data is a ComponentsDriver (kept while the actor
    // stays selected); tabs, folds and the Add Component search are each Inspector's ComponentsViewState.
    [InspectorTool("Components", Icon = "Inspector", Priority = 1000, Tooltip = "Components of the selected actor")]
    public sealed class ComponentsInspectorTool : IInspectorTool
    {
        private const float StripSize = 30.0f;
        private const float TabSize = 28.0f;
        private const float TabGap = 2.0f;
        private const float AddPopupWidth = 240.0f;
        private const float AddRowHeight = 22.0f;
        private const float AddListMaxHeight = 300.0f;
        private const string AddPopupId = "inspector_add";
        private const float ActorIconBox = 40.0f;
        private const float ActorIconSize = 28.0f;

        private static readonly IReadOnlyList<object> _none = Array.Empty<object>();

        public ToolUsability OnSelectItems(object[] selection) => ToolUsability.Usable;

        public object? DriverKey(ToolSelection selection) => selection.Last<ActorID>();

        public IToolDriver? CreateDriver(ToolSelection selection) =>
            selection.Last<ActorID>() is ActorID actor ? new ComponentsDriver(actor) : null;

        public void OnInspectorGUI(InspectorToolContext context)
        {
            ComponentsViewState view = context.View.State<ComponentsViewState>();
            ComponentsDriver? driver = context.Driver as ComponentsDriver;
            bool hasActor = driver is { Exists: true };
            if (!hasActor)
                driver = null;
            if (driver is not null && view.LockWhenShowing)
            {
                view.LockWhenShowing = false;
                context.LockAndFocus();
            }
            if (driver is null)
            {
                view.Shown = 0;
                view.Tab = 0;
            }
            else
            {
                if (driver.Actor.Value != view.Shown)
                {
                    view.Shown = driver.Actor.Value;
                    if (!view.KeepTab)
                        view.Tab = 0;
                    view.KeepTab = false;
                }
                if (view.PendingAddType != null)
                {
                    if (driver.AddComponent(view.PendingAddType))
                        view.SelectLast = true;
                    view.PendingAddType = null;
                }
                driver.Refresh();
                if (view.SelectLast)
                {
                    view.Tab = driver.Components.Count;
                    view.SelectLast = false;
                }
            }
            IReadOnlyList<object> components = driver?.Components ?? _none;
            if (view.Tab > components.Count)
                view.Tab = 0;
            if (view.OpenAddDelay >= 0 && view.OpenAddDelay-- == 0 && hasActor)
                view.OpenAdd = true;

            bool stripRight = TabStrip.Detect(Gui.PanelPlacement) == PanelSide.Left;
            bool readOnly = driver is not null && ActorEditorState.IsLockedInHierarchy(driver.Actor);
            using (Gui.Horizontal(0.0f))
            {
                if (!stripRight)
                    DrawTabs(view, driver, components, TabEdge.Left, hasActor, readOnly);
                DrawContent(context, view, driver, readOnly);
                if (stripRight)
                    DrawTabs(view, driver, components, TabEdge.Right, hasActor, readOnly);
            }
            DrawAddPopup(view, driver, stripRight);
            DrawCopyPopup(view);
            if (driver is not null && view.MenuHookFrames >= 0 && ++view.MenuHookFrames == 240
                && int.TryParse(Environment.GetEnvironmentVariable("XG_UI_TEST_COMPONENT_MENU"), out int hookIndex) && hookIndex >= 0 && hookIndex < components.Count)
            {
                view.MenuHookFrames = -1;
                view.Menu.Open(new ContextMenuContext(ComponentContextMenu.Source, new object[] { driver.Actor, components[hookIndex] }, driver), new Vector2(view.AddTab.max.x + 40.0f, view.AddTab.min.y - 80.0f));
            }
            view.Menu.OnGUI();
            if (driver is not null && view.IconHookFrames >= 0 && ++view.IconHookFrames == 170 && IconPicker.ParseHook() is { Target: "inspector" } iconHook)
            {
                view.IconHookFrames = -1;
                OpenIconPicker(view, driver.Actor, new Vector2(40.0f, 60.0f));
                view.IconPicker.Hook(iconHook.Search, iconHook.Pick);
                Debug.Log("[Debug] Icon picker opened from the Inspector");
            }
            view.IconPicker.OnGUI();
        }

        // The actor's icon left of its name: a button that opens the icon picker (not for locked or Composition Actors).
        private static void DrawActorIcon(ComponentsViewState view, ActorID actor)
        {
            bool composed = ArchetypeDriver.IsCompositionActor(actor);
            bool editable = EditorIcons.CanEdit(actor);
            using var canvas = Gui.Canvas("##actor_icon", new Vector2(ActorIconBox, ActorIconBox));
            Painter draw = canvas.Draw;
            Rect box = Rect.FromXYWH(0.0f, 0.0f, ActorIconBox, ActorIconBox);
            Result hit = Gui.HitArea("##actor_icon_hit", box);
            bool hovered = editable && hit.Hovered();
            if (hovered)
            {
                draw.RectFilled(box, Style.Color(StyleColor.ButtonHover), 3.0f);
                draw.Rect(box, Style.Color(StyleColor.Accent), 3.0f);
            }
            ComponentIcon icon = composed ? new ComponentIcon(Icon.Actor, null, HierarchyStyle.Composed) : EditorIcons.OfActor(actor);
            icon.Draw(draw, box.Center, ActorIconSize);
            Gui.Tooltip(composed ? "Composition Actors keep their archetype icon" : editable ? "Click to change the actor's icon" : "The actor is locked");
            if (editable && hit.Clicked())
                OpenIconPicker(view, actor, null);
        }

        // The actor's id in dim text under its name.
        private static void DrawActorId(ActorID actor)
        {
            string text = $"ID {actor.Value}";
            Vector2 size = Gui.MeasureText(text);
            using var canvas = Gui.Canvas("##actor_id", new Vector2(size.x, size.y));
            canvas.Draw.Text(new Vector2(0.0f, 0.0f), Style.Color(StyleColor.TextDim), text);
        }

        private static void OpenIconPicker(ComponentsViewState view, ActorID actor, Vector2? position)
        {
            if (!EditorIcons.CanEdit(actor))
                return;
            ActorID[] targets = Selection.Contains(actor) ? Selection.Actors.ToArray() : new[] { actor };
            view.IconPicker.Open(ActorEditorState.GetIcon(actor), Icon.Actor, icon => EditorIcons.SetActors(targets, icon), position);
        }

        private static void OpenComponentMenu(ComponentsViewState view, ComponentsDriver driver, object component) =>
            view.Menu.Open(new ContextMenuContext(ComponentContextMenu.Source, new object[] { driver.Actor, component }, driver));

        // ------------------------------------------------------------
        // Copy value (read-only actors)
        // ------------------------------------------------------------
        private static void RequestCopy(ComponentsViewState view, string label, string text)
        {
            view.CopyLabel = label;
            view.CopyText = text;
            view.OpenCopy = true;
        }

        private static void DrawCopyPopup(ComponentsViewState view)
        {
            const string id = "inspector_copy_value";
            if (view.OpenCopy)
            {
                view.OpenCopy = false;
                view.CopyPosition = Gui.MousePosition;
                Gui.OpenPopup(id);
            }
            using var popup = Gui.Popup(id, view.CopyPosition, new Vector2(ContextMenu.Width, 0.0f));
            if (!popup.IsOpen || view.CopyText is not string text)
                return;
            using var canvas = Gui.Canvas("##copy_rows", new Vector2(ContextMenu.Width, ContextMenu.RowHeight + ContextMenu.Padding * 2.0f));
            Rect row = Rect.FromXYWH(ContextMenu.Padding, ContextMenu.Padding, ContextMenu.Width - ContextMenu.Padding * 2.0f, ContextMenu.RowHeight);
            if (ToggleButton.Draw(canvas.Draw, "##copy", row, Icon.File, null, "Copy Value", null, $"{view.CopyLabel}: {text}", false, true, ToggleDot.None).Clicked())
            {
                Gui.SetClipboard(text);
                Debug.Log($"[Debug] Inspector copied {view.CopyLabel}: {text}");
                Gui.ClosePopup();
            }
        }

        // ------------------------------------------------------------
        // Tabs
        // ------------------------------------------------------------
        private static void DrawTabs(ComponentsViewState view, ComponentsDriver? driver, IReadOnlyList<object> components, TabEdge edge, bool hasActor, bool readOnly)
        {
            using var canvas = Gui.Canvas("##inspector_tabs", new Vector2(StripSize, 0.0f));
            Painter draw = canvas.Draw;
            draw.RectFilled(new Rect(new Vector2(0.0f, 0.0f), canvas.Size), Style.Color(StyleColor.TopBarBg));
            float iconX = edge == TabEdge.Left
                ? TabStrip.AccentWidth + (StripSize - TabStrip.AccentWidth) * 0.5f
                : (StripSize - TabStrip.AccentWidth) * 0.5f;

            float y = TabGap + 2.0f;
            DrawTab(view, draw, 0, Rect.FromXYWH(0.0f, y, StripSize, TabSize), edge, "All components", icon => icon.Draw(draw, new Vector2(iconX, y + TabSize * 0.5f), 15.0f),
                new ComponentIcon(Icon.Cards, null, Style.Color(StyleColor.Icon)), null);
            y += TabSize + TabGap + 2.0f;
            draw.Line(new Vector2(6.0f, y), new Vector2(StripSize - 6.0f, y), Style.Color(StyleColor.Border));
            y += 3.0f;

            for (int i = 0; i < components.Count; i++)
            {
                Type type = TypeBridge.ToEditor(components[i].GetType());
                ComponentIcon icon = ComponentIcons.Get(type);
                float top = y;
                object component = components[i];
                DrawTab(view, draw, i + 1, Rect.FromXYWH(0.0f, top, StripSize, TabSize), edge, SerializedProperty.Nicify(type.Name),
                    resolved => resolved.Draw(draw, new Vector2(iconX, top + TabSize * 0.5f), 15.0f), icon,
                    driver is null ? null : () => OpenComponentMenu(view, driver, component));
                y += TabSize + TabGap;
            }

            y += 4.0f;
            view.AddTab = Rect.FromXYWH(canvas.Bounds.min.x, canvas.Bounds.min.y + y, StripSize, TabSize);
            Rect add = Rect.FromXYWH(0.0f, y, StripSize, TabSize);
            Result hit;
            using (Gui.Id(-1))
                hit = Gui.HitArea("##add", add);
            bool canAdd = hasActor && !readOnly;
            Gui.Tooltip(canAdd ? "Add Component" : readOnly ? "The actor is locked" : "Select an actor to add components");
            TabStrip.DrawShape(draw, add, edge, false, canAdd && hit.Hovered());
            Color32 plus = Style.Color(canAdd ? StyleColor.Text : StyleColor.TextDim);
            draw.Icon(Icon.Plus, new Vector2(iconX, y + TabSize * 0.5f), 14.0f, plus);
            if (canAdd && hit.Clicked())
                view.OpenAdd = true;
        }

        private static void DrawTab(ComponentsViewState view, Painter draw, int index, Rect tab, TabEdge edge, string tooltip, Action<ComponentIcon> drawIcon, ComponentIcon icon,
            Action? rightClicked)
        {
            Result hit;
            using (Gui.Id(index))
                hit = Gui.HitArea("##tab", tab);
            Gui.Tooltip(tooltip);
            if (hit.Clicked())
                view.Tab = index;
            if ((hit & Result.RightClicked) != 0)
                rightClicked?.Invoke();
            TabStrip.DrawShape(draw, tab, edge, index == view.Tab, hit.Hovered());
            drawIcon(icon);
        }

        // ------------------------------------------------------------
        // Cards
        // ------------------------------------------------------------
        private static void DrawContent(InspectorToolContext context, ComponentsViewState view, ComponentsDriver? driver, bool readOnly)
        {
            using var area = Gui.ScrollArea("##inspector_content", default, ScrollAreaFlags.Padded);
            if (!area.IsOpen)
                return;
            Vector2 avail = Gui.AvailableSize;
            Gui.Draw.RectFilled(new Rect(new Vector2(-6.0f, -6.0f), new Vector2(avail.x + 6.0f, 100000.0f)), Style.Color(StyleColor.ToolbarBg));

            using (Gui.Horizontal())
            {
                if (driver is not null)
                {
                    DrawActorIcon(view, driver.Actor);
                    using (Gui.Vertical())
                    {
                        using (Gui.Horizontal())
                        {
                            if (readOnly)
                                DrawLockedMark();
                            using (Gui.Font(FontStyle.Bold))
                                Gui.Label(driver.Name);
                        }
                        DrawActorId(driver.Actor);
                    }
                    Gui.FlexibleSpace();
                    if (readOnly)
                        Gui.Label("read only");
                }
                else
                {
                    Gui.Label(context.Locked ? "The locked selection has no actor." : "Select an actor in the Hierarchy.");
                    Gui.FlexibleSpace();
                }
                context.DrawLock();
            }
            if (driver is null)
                return;
            Gui.Space(2.0f);

            IReadOnlyList<object> components = driver.Components;
            if (components.Count == 0)
            {
                Gui.Label("No components.");
                return;
            }

            EditorGUI.RowContext = readOnly ? (label, text) => RequestCopy(view, label, text) : null;
            for (int i = 0; i < components.Count; i++)
            {
                if (view.Tab != 0 && view.Tab != i + 1)
                    continue;
                object component = components[i];
                Type type = TypeBridge.ToEditor(component.GetType());
                ComponentIcon icon = ComponentIcons.Get(type);
                string title = SerializedProperty.Nicify(type.Name);
                using (Gui.Id(i))
                {
                    if (view.Tab == 0)
                    {
                        string key = $"{TypeBridge.KeyOf(type)}#{i}";
                        bool open = !view.Open.TryGetValue(key, out bool stored) || stored;
                        using (var card = Gui.Card("##card", title, icon.Icon, icon.Texture, icon.Color, ref open))
                        {
                            if (Gui.TakeCardRightClick())
                                OpenComponentMenu(view, driver, component);
                            if (card.IsOpen)
                                DrawBody(view, driver, component, readOnly);
                        }
                        view.Open[key] = open;
                        if (open)
                            DrawPreview(type, component);
                    }
                    else
                    {
                        using var card = Gui.Card("##card", title, icon.Icon, icon.Texture, icon.Color);
                        if (Gui.TakeCardRightClick())
                            OpenComponentMenu(view, driver, component);
                        DrawBody(view, driver, component, readOnly);
                    }
                    if (view.Tab != 0)
                        DrawPreview(type, component);
                }
                Gui.Space(2.0f);
            }
        }

        // The optional preview card under a component's card (ComponentEditors.Preview).
        private static void DrawPreview(Type type, object component)
        {
            IComponentPreview? preview = ComponentEditors.Preview(type);
            if (preview is null || component is not Component owned || !owned.IsAttached)
                return;
            Gui.Space(2.0f);
            using var card = Gui.Card("##preview", preview.Title, preview.Icon, null, Style.Color(StyleColor.TextDim));
            preview.OnGUI(owned.ActorID, component);
        }

        // The amber lock in front of a locked actor's name.
        private static void DrawLockedMark()
        {
            using var canvas = Gui.Canvas("##locked_mark", new Vector2(16.0f, Gui.MeasureText("A").y));
            canvas.Draw.Icon(Icon.Lock, new Vector2(8.0f, canvas.Size.y * 0.5f), 12.0f, HierarchyStyle.Lock);
            Gui.Tooltip("Locked in the Hierarchy: the actor can be browsed and copied, not edited");
        }

        private static void DrawBody(ComponentsViewState view, ComponentsDriver driver, object component, bool readOnly)
        {
            using var disabled = Gui.Disabled(readOnly);
            if (component is Transform transform)
            {
                DrawTransform(view, transform, readOnly);
                return;
            }
            SerializedObject serialized = driver.Serialized(component);
            if (component is ComponentDataSet set)
            {
                if (serialized.Properties.Count > 0)
                    EditorGUI.ObjectFields(serialized);
                DrawData(set, readOnly);
                return;
            }
            DrawFields(serialized);
        }

        private static void DrawData(ComponentDataSet set, bool readOnly)
        {
            for (int i = 0; i < set.DataCount; i++)
            {
                object? boxed = set.GetData(i);
                IComponentDataEditor? editor = ComponentEditors.DataEditor(set.DataType(i));
                if (editor is not null && boxed is not null)
                {
                    object result;
                    using (Gui.Id(1000 + i))
                        result = editor.OnGUI(set.ActorID, boxed);
                    if (!readOnly && !ReferenceEquals(result, boxed))
                        set.SetData(i, result);
                    continue;
                }
                using (Gui.Font(FontStyle.Bold))
                    Gui.Label(SerializedProperty.Nicify(set.DataType(i).Name));
                if (boxed is null)
                {
                    Gui.Label("Not available.");
                    continue;
                }
                using (Gui.Id(1000 + i))
                    EditorGUI.ObjectFields(new SerializedObject(boxed));
                set.SetData(i, boxed);
            }
        }

        private static bool TransformField(string label, ref Vector3 value, NumberFieldStyle style, float speed)
        {
            Span<float> v = stackalloc float[3] { value.x, value.y, value.z };
            if (!Gui.Field(label, v, style, speed))
                return false;
            value = new Vector3(v[0], v[1], v[2]);
            return true;
        }

        private static void DrawTransform(ComponentsViewState view, Transform transform, bool readOnly)
        {
            if (!transform.IsValid)
            {
                Gui.Label("The transform was removed.");
                return;
            }
            if (ArchetypeDriver.IsCompositionActor(transform.ActorID))
            {
                using (Gui.Horizontal(4.0f))
                {
                    using (var mark = Gui.Canvas("##jointer_mark", new Vector2(16.0f, Gui.MeasureText("A").y)))
                        mark.Draw.Icon(Icon.Link, new Vector2(8.0f, mark.Size.y * 0.5f), 12.0f, HierarchyStyle.Composed);
                    Gui.Label($"Driven by {ActorTree.GetName(ArchetypeDriver.CompositionOwner(transform.ActorID))}'s archetype Jointer (edits snap back)");
                }
            }
            TransformData local = transform.Local;
            Quaternion rotation = local.Rotation;
            if (view.EulerActor != transform.ActorID.Value || !SameRotation(view.EulerSource, rotation))
            {
                view.Euler = Transform.ToEuler(rotation);
                view.EulerSource = rotation;
                view.EulerActor = transform.ActorID.Value;
            }

            Vector3 position = local.Position;
            Vector3 euler = view.Euler;
            Vector3 scale = local.Scale;
            bool changed = false;
            Gui.TakeRowRightClick();
            if (TransformField("Position", ref position, NumberFieldStyle.Tagged("x,y,z"), 0.1f))
            {
                local.Position = position;
                changed = true;
            }
            if (Gui.TakeRowRightClick() && readOnly)
                RequestCopy(view, "Position", EditorGUI.ValueText(position));
            if (TransformField("Rotation", ref euler, NumberFieldStyle.Ranged(-180.0f, 180.0f, "x,y,z"), 0.5f))
            {
                view.Euler = euler;
                view.EulerSource = Transform.FromEuler(euler);
                local.Rotation = view.EulerSource;
                changed = true;
            }
            if (Gui.TakeRowRightClick() && readOnly)
                RequestCopy(view, "Rotation", EditorGUI.ValueText(euler));
            if (TransformField("Scale", ref scale, NumberFieldStyle.Tagged("x,y,z"), 0.01f))
            {
                local.Scale = scale;
                changed = true;
            }
            if (Gui.TakeRowRightClick() && readOnly)
                RequestCopy(view, "Scale", EditorGUI.ValueText(scale));
            if (changed && !readOnly)
                transform.Local = local;

            Vector3 world = transform.Position;
            Gui.PropertyLabel("World position", $"{world.x:0.###}, {world.y:0.###}, {world.z:0.###}");
        }

        private static bool SameRotation(Quaternion a, Quaternion b)
        {
            const float epsilon = 1e-5f;
            return MathF.Abs(a.x - b.x) < epsilon && MathF.Abs(a.y - b.y) < epsilon && MathF.Abs(a.z - b.z) < epsilon && MathF.Abs(a.w - b.w) < epsilon;
        }

        private static void DrawFields(SerializedObject serialized)
        {
            if (serialized.Properties.Count == 0)
                Gui.Label("No visible fields.");
            EditorGUI.ObjectFields(serialized);
        }

        // ------------------------------------------------------------
        // Add Component
        // ------------------------------------------------------------
        private static void DrawAddPopup(ComponentsViewState view, ComponentsDriver? driver, bool stripRight)
        {
            if (view.OpenAdd)
            {
                view.OpenAdd = false;
                ComponentCatalog.Invalidate();
                view.Search.Text = string.Empty;
                Gui.OpenPopup(AddPopupId);
            }
            Rect addTab = view.AddTab;
            Vector2 position = stripRight
                ? new Vector2(addTab.min.x - AddPopupWidth - 2.0f, addTab.min.y)
                : new Vector2(addTab.max.x + 2.0f, addTab.min.y);
            using var popup = Gui.Popup(AddPopupId, position, new Vector2(AddPopupWidth, 0.0f));
            if (!popup.IsOpen)
                return;

            Gui.Space(4.0f);
            using (Gui.Horizontal())
            {
                Gui.Space(2.0f);
                Gui.SearchField("##add_search", view.Search, 0.0f);
                Gui.Space(2.0f);
            }

            string filter = view.Search.Text.Trim();
            List<ComponentType> filtered = view.Filtered;
            filtered.Clear();
            bool hasTransform = driver is not null && Transform.Exists(driver.Actor);
            foreach (ComponentType type in ComponentCatalog.Types)
            {
                if (hasTransform && type.Type == typeof(Transform))
                    continue;
                if (filter.Length == 0 || type.Name.Contains(filter, StringComparison.OrdinalIgnoreCase))
                    filtered.Add(type);
            }

            if (filtered.Count == 0)
            {
                Gui.Label(" No matching components.");
                Gui.Space(4.0f);
                return;
            }

            float listHeight = filtered.Count * (AddRowHeight + 3.0f) + 6.0f;
            using var list = Gui.ScrollArea("##add_list", new Vector2(AddPopupWidth, MathF.Min(listHeight, AddListMaxHeight)));
            if (!list.IsOpen)
                return;
            float width = MathF.Max(Gui.AvailableSize.x, 1.0f);
            using var canvas = Gui.Canvas("##add_items", new Vector2(width, listHeight));
            Painter draw = canvas.Draw;
            ComponentType? picked = null;
            for (int i = 0; i < filtered.Count; i++)
            {
                ComponentType type = filtered[i];
                Rect row = Rect.FromXYWH(4.0f, 3.0f + i * (AddRowHeight + 3.0f), width - 8.0f, AddRowHeight);
                Result hit;
                using (Gui.Id(i))
                    hit = Gui.HitArea("##add_item", row);
                Gui.Tooltip(type.Key);
                Color32 fill = Style.Color(hit.Hovered() ? StyleColor.ButtonHover : StyleColor.Button);
                draw.RectFilled(row, Style.Color(StyleColor.SegmentBg), 3.0f);
                draw.RectFilled(new Rect(row.min, new Vector2(row.max.x, row.max.y - 1.0f)), fill, 3.0f);
                float cy = row.min.y + row.Height * 0.5f;
                ComponentIcons.Get(type.Type).Draw(draw, new Vector2(row.min.x + 15.0f, cy), 14.0f);
                draw.Text(new Vector2(row.min.x + 30.0f, MathF.Round(cy - Gui.MeasureText(type.Name).y * 0.5f)), Style.Color(StyleColor.Text), type.Name);
                if (hit.Clicked())
                    picked = type;
            }
            if (picked != null && driver is not null)
            {
                if (driver.AddComponent(picked.Type))
                {
                    if (picked.Type == typeof(Transform))
                        view.SelectTab(1);
                    else
                        view.SelectLast = true;
                    Gui.ClosePopup();
                }
                else
                {
                    Debug.LogError($"[Inspector] Could not add {picked.Name} to {driver.Name}.");
                }
            }
        }
    }
}
