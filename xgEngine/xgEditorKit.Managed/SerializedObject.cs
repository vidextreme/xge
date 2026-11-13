// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Reflection;

namespace xg.editor
{
    // The Inspector's view of a live object: its visible fields as SerializedProperty objects. A field is visible when
    // it is public or marked [SerializeField], and not [HideInInspector], static, const or readonly.
    public sealed class SerializedObject
    {
        private readonly List<SerializedProperty> _properties = new();

        public SerializedObject(object target)
        {
            Target = target;
            foreach ((FieldInfo live, FieldInfo editor) in VisibleFields(target.GetType()))
                _properties.Add(new SerializedProperty(target, [live], [editor]));
        }

        public object Target { get; }
        public Type TargetType => TypeBridge.ToEditor(Target.GetType());
        public IReadOnlyList<SerializedProperty> Properties => _properties;

        public SerializedProperty? FindProperty(string name)
        {
            foreach (SerializedProperty property in _properties)
                if (property.Name == name)
                    return property;
            return null;
        }

        internal static IEnumerable<(FieldInfo Live, FieldInfo Editor)> VisibleFields(Type liveType)
        {
            List<Type> chain = new();
            for (Type? t = liveType; t is not null && t != typeof(object) && t != typeof(ValueType); t = t.BaseType)
                chain.Insert(0, t);

            const BindingFlags flags = BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.DeclaredOnly;
            foreach (Type type in chain)
            {
                Type editorDeclaring = TypeBridge.ToEditor(type);
                foreach (FieldInfo live in type.GetFields(flags))
                {
                    if (live.IsInitOnly || live.IsLiteral || !IsVisible(live))
                        continue;
                    FieldInfo editor = editorDeclaring.GetField(live.Name, flags) ?? live;
                    yield return (live, editor);
                }
            }
        }

        private static bool IsVisible(FieldInfo field)
        {
            if (TypeBridge.HasAttribute(field, typeof(HideInInspectorAttribute)))
                return false;
            if (TypeBridge.HasAttribute(field, typeof(SerializeFieldAttribute)))
                return true;
            return field.IsPublic;
        }
    }
}
