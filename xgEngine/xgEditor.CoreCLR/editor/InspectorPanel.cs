// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg;
using xg.editor;
using xg.imui;

namespace xgEditor.CoreCLR
{
    // Editor Inspector: the components of the actor selected in the Hierarchy, each in a foldout, with their fields
    // drawn by PropertyDrawers (built-in ones, or a project's own from its Editor scripts). Edits apply immediately to
    // the live components, which may belong to any CoreCLR module (the editor's or the game project's).
    public sealed class InspectorPanel : Panel
    {
        private readonly List<object> _components = new();
        private readonly Dictionary<object, SerializedObject> _serialized = new(ReferenceEqualityComparer.Instance);
        private readonly Dictionary<string, bool> _open = new();
        private HierarchyPanel? _hierarchy;
        private ulong _shown;

        public override string Title => "Inspector";
        public override string? MenuPath => "General/Inspector";
        public override DockSlot Slot => DockSlot.Right;
        public override Icon PanelIcon => Icon.Inspector;

        // Follows the selection of this Hierarchy.
        public void Bind(HierarchyPanel hierarchy) => _hierarchy = hierarchy;

        protected override void OnGUI()
        {
            ActorID? selected = _hierarchy?.Selected;
            if (selected is not ActorID actor || ActorTree.GetName(actor).Length == 0)
            {
                _serialized.Clear();
                _shown = 0;
                Gui.Label("Select an actor in the Hierarchy.");
                return;
            }
            if (actor.Value != _shown)
            {
                _serialized.Clear();
                _shown = actor.Value;
            }

            using (Gui.Font(FontStyle.Bold))
                Gui.Label(ActorTree.GetName(actor));
            Gui.PropertyLabel("Actor ID", actor.Value.ToString());
            Gui.Space(4.0f);

            ActorTree.GetComponents(actor, _components);
            if (_components.Count == 0)
            {
                Gui.Label("No components.");
                return;
            }

            foreach (object component in _components)
            {
                Type type = TypeBridge.ToEditor(component.GetType());
                string key = TypeBridge.KeyOf(type);
                bool open = !_open.TryGetValue(key, out bool stored) || stored;
                using (Gui.Id(key))
                {
                    if (Gui.Foldout(SerializedProperty.Nicify(type.Name), ref open))
                    {
                        if (!_serialized.TryGetValue(component, out SerializedObject? serialized))
                        {
                            serialized = new SerializedObject(component);
                            _serialized[component] = serialized;
                        }
                        if (serialized.Properties.Count == 0)
                            Gui.Label("No visible fields.");
                        EditorGUI.ObjectFields(serialized);
                        Gui.Space(4.0f);
                    }
                }
                _open[key] = open;
            }
        }
    }
}
