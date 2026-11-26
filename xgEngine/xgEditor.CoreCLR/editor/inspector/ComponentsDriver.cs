// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg;
using xg.editor;
using xg.math;

namespace xgEditor.CoreCLR
{
    // The data of the Components Inspector tool for one actor: its live components (from every CoreCLR module), a
    // SerializedObject per component, and adding components. Edits apply immediately, so nothing is ever pending.
    public sealed class ComponentsDriver : IToolDriver
    {
        private readonly List<object> _components = new();
        private readonly Dictionary<object, SerializedObject> _serialized = new(ReferenceEqualityComparer.Instance);

        public ComponentsDriver(ActorID actor) => Actor = actor;

        public ActorID Actor { get; }
        public bool Exists => Actor != ActorID.InvalidID && ActorTree.GetName(Actor).Length > 0;
        public string Name => ActorTree.GetName(Actor);
        public IReadOnlyList<object> Components => _components;

        // Re-reads the actor's components (they can change any frame).
        public void Refresh()
        {
            if (Exists)
                ActorTree.GetComponents(Actor, _components);
            else
                _components.Clear();
        }

        public SerializedObject Serialized(object component)
        {
            if (!_serialized.TryGetValue(component, out SerializedObject? serialized))
            {
                serialized = new SerializedObject(component);
                _serialized[component] = serialized;
            }
            return serialized;
        }

        public bool AddComponent(Type type) => Exists && ActorTree.AddComponent(Actor, type);

        // Adds the type with this full name from any loaded module, listed in the component catalog or not (test hook).
        public bool AddComponent(string fullName)
        {
            foreach (System.Runtime.Loader.AssemblyLoadContext context in System.Runtime.Loader.AssemblyLoadContext.All)
            {
                foreach (System.Reflection.Assembly assembly in context.Assemblies)
                {
                    Type? type = assembly.IsDynamic ? null : assembly.GetType(fullName, false);
                    if (type is null)
                        continue;
                    bool added = AddComponent(type);
                    Debug.Log($"[Debug] Inspector add {TypeBridge.KeyOf(type)} to {Name}: {(added ? "added" : "failed")}");
                    return added;
                }
            }
            Debug.LogWarning($"[Debug] Inspector add: no type named {fullName}");
            return false;
        }

        public void Dispose()
        {
            _serialized.Clear();
            _components.Clear();
        }
    }

    // Per-Inspector UI state of the Components tool.
    public sealed class ComponentsViewState
    {
        // 0 = All, n = the n-th component.
        public int Tab;
        public bool KeepTab;
        public bool SelectLast;
        public ulong Shown;
        public Rect AddTab;
        public bool OpenAdd;
        // Copy value menu of a read-only (locked) actor's row: opened at panel level the frame after the right-click.
        public string? CopyLabel;
        public string? CopyText;
        public bool OpenCopy;
        public Vector2 CopyPosition;
        public int OpenAddDelay = -1;
        public string? PendingAddType;
        public bool LockWhenShowing;
        public readonly Dictionary<string, bool> Open = new();
        public readonly List<ComponentType> Filtered = new();
        public readonly xg.imui.TextBuffer Search = new(64);
        // Rotation shown as Euler angles, kept while the transform's rotation stays the one they produced.
        public Vector3 Euler;
        public Quaternion EulerSource;
        public ulong EulerActor;

        public void SelectTab(int tab)
        {
            Tab = Math.Max(tab, 0);
            KeepTab = true;
        }
    }
}
