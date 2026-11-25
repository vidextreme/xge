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
            using (Gui.Horizontal(0.0f))
            {
                if (!stripRight)
                    DrawTabs(view, components, TabEdge.Left, hasActor);
                DrawContent(context, view, driver);
                if (stripRight)
                    DrawTabs(view, components, TabEdge.Right, hasActor);
            }
            DrawAddPopup(view, driver, stripRight);
        }

        // ------------------------------------------------------------
        // Tabs
        // ------------------------------------------------------------
        private static void DrawTabs(ComponentsViewState view, IReadOnlyList<object> components, TabEdge edge, bool hasActor)
        {
            using var canvas = Gui.Canvas("##inspector_tabs", new Vector2(StripSize, 0.0f));
            Painter draw = canvas.Draw;
            draw.RectFilled(new Rect(new Vector2(0.0f, 0.0f), canvas.Size), Style.Color(StyleColor.TopBarBg));
            float iconX = edge == TabEdge.Left
                ? TabStrip.AccentWidth + (StripSize - TabStrip.AccentWidth) * 0.5f
                : (StripSize - TabStrip.AccentWidth) * 0.5f;

            float y = TabGap + 2.0f;
            DrawTab(view, draw, 0, Rect.FromXYWH(0.0f, y, StripSize, TabSize), edge, "All components", icon => icon.Draw(draw, new Vector2(iconX, y + TabSize * 0.5f), 15.0f),
                new ComponentIcon(Icon.Cards, null, Style.Color(StyleColor.Icon)));
            y += TabSize + TabGap + 2.0f;
            draw.Line(new Vector2(6.0f, y), new Vector2(StripSize - 6.0f, y), Style.Color(StyleColor.Border));
            y += 3.0f;

            for (int i = 0; i < components.Count; i++)
            {
                Type type = TypeBridge.ToEditor(components[i].GetType());
                ComponentIcon icon = ComponentIcons.Get(type);
                float top = y;
                DrawTab(view, draw, i + 1, Rect.FromXYWH(0.0f, top, StripSize, TabSize), edge, SerializedProperty.Nicify(type.Name),
                    resolved => resolved.Draw(draw, new Vector2(iconX, top + TabSize * 0.5f), 15.0f), icon);
                y += TabSize + TabGap;
            }

            y += 4.0f;
            view.AddTab = Rect.FromXYWH(canvas.Bounds.min.x, canvas.Bounds.min.y + y, StripSize, TabSize);
            Rect add = Rect.FromXYWH(0.0f, y, StripSize, TabSize);
            Result hit;
            using (Gui.Id(-1))
                hit = Gui.HitArea("##add", add);
            Gui.Tooltip(hasActor ? "Add Component" : "Select an actor to add components");
            TabStrip.DrawShape(draw, add, edge, false, hasActor && hit.Hovered());
            Color32 plus = Style.Color(hasActor ? StyleColor.Text : StyleColor.TextDim);
            draw.Icon(Icon.Plus, new Vector2(iconX, y + TabSize * 0.5f), 14.0f, plus);
            if (hasActor && hit.Clicked())
                view.OpenAdd = true;
        }

        private static void DrawTab(ComponentsViewState view, Painter draw, int index, Rect tab, TabEdge edge, string tooltip, Action<ComponentIcon> drawIcon, ComponentIcon icon)
        {
            Result hit;
            using (Gui.Id(index))
                hit = Gui.HitArea("##tab", tab);
            Gui.Tooltip(tooltip);
            if (hit.Clicked())
                view.Tab = index;
            TabStrip.DrawShape(draw, tab, edge, index == view.Tab, hit.Hovered());
            drawIcon(icon);
        }

        // ------------------------------------------------------------
        // Cards
        // ------------------------------------------------------------
        private static void DrawContent(InspectorToolContext context, ComponentsViewState view, ComponentsDriver? driver)
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
                    using (Gui.Font(FontStyle.Bold))
                        Gui.Label(driver.Name);
                    Gui.FlexibleSpace();
                    Gui.Label($"ID {driver.Actor.Value}");
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
                            if (card.IsOpen)
                                DrawBody(view, driver, component);
                        }
                        view.Open[key] = open;
                    }
                    else
                    {
                        using var card = Gui.Card("##card", title, icon.Icon, icon.Texture, icon.Color);
                        DrawBody(view, driver, component);
                    }
                }
                Gui.Space(2.0f);
            }
        }

        private static void DrawBody(ComponentsViewState view, ComponentsDriver driver, object component)
        {
            if (component is Transform transform)
            {
                DrawTransform(view, transform);
                return;
            }
            SerializedObject serialized = driver.Serialized(component);
            if (component is ComponentDataSet set)
            {
                if (serialized.Properties.Count > 0)
                    EditorGUI.ObjectFields(serialized);
                DrawData(set);
                return;
            }
            DrawFields(serialized);
        }

        private static void DrawData(ComponentDataSet set)
        {
            for (int i = 0; i < set.DataCount; i++)
            {
                object? boxed = set.GetData(i);
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

        private static void DrawTransform(ComponentsViewState view, Transform transform)
        {
            if (!transform.IsValid)
            {
                Gui.Label("The transform was removed.");
                return;
            }
            if (ObjectDriverComponent.IsCompositionActor(transform.ActorID))
                Gui.Label($"Driven by {ActorTree.GetName(ObjectDriverComponent.CompositionOwner(transform.ActorID))}'s composition (edits snap back)");
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
            if (TransformField("Position", ref position, NumberFieldStyle.Tagged("x,y,z"), 0.1f))
            {
                local.Position = position;
                changed = true;
            }
            if (TransformField("Rotation", ref euler, NumberFieldStyle.Ranged(-180.0f, 180.0f, "x,y,z"), 0.5f))
            {
                view.Euler = euler;
                view.EulerSource = Transform.FromEuler(euler);
                local.Rotation = view.EulerSource;
                changed = true;
            }
            if (TransformField("Scale", ref scale, NumberFieldStyle.Tagged("x,y,z"), 0.01f))
            {
                local.Scale = scale;
                changed = true;
            }
            if (changed)
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
