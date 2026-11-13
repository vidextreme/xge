// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Reflection;
using System.Text;
using xg.math;

namespace xg.editor
{
    // One field of a live object (a component), possibly inside nested structs. Reads and writes go to the live object,
    // and struct fields are written back up the path. FieldType and FieldInfo are the editor's copies, so drawers can
    // compare them with typeof(...) and cast attributes to their own types.
    public sealed class SerializedProperty
    {
        private readonly object _target;
        private readonly FieldInfo[] _livePath;
        private readonly FieldInfo[] _editorPath;
        private List<SerializedProperty>? _children;

        internal SerializedProperty(object target, FieldInfo[] livePath, FieldInfo[] editorPath)
        {
            _target = target;
            _livePath = livePath;
            _editorPath = editorPath;
            DisplayName = Nicify(Name);
        }

        public object Target => _target;
        public FieldInfo FieldInfo => _editorPath[^1];
        public Type FieldType => FieldInfo.FieldType;
        public string Name => FieldInfo.Name;
        public string DisplayName { get; }
        public string PropertyPath => string.Join(".", _editorPath.Select(f => f.Name));
        public int Depth => _editorPath.Length - 1;

        // The value converted to the editor's copy of FieldType.
        public object? Value
        {
            get => TypeBridge.Convert(LiveValue, FieldType);
            set => LiveValue = TypeBridge.Convert(value, _livePath[^1].FieldType);
        }

        public float FloatValue { get => System.Convert.ToSingle(LiveValue); set => Value = value; }
        public int IntValue { get => System.Convert.ToInt32(LiveValue); set => Value = value; }
        public bool BoolValue { get => LiveValue is true; set => Value = value; }
        public string StringValue { get => LiveValue as string ?? string.Empty; set => Value = value; }
        public Vector2 Vector2Value { get => Value is Vector2 v ? v : default; set => Value = value; }
        public Vector3 Vector3Value { get => Value is Vector3 v ? v : default; set => Value = value; }

        // Enum fields: the value's index in EnumNames (-1 when the value is not a named constant).
        public string[] EnumNames => FieldType.IsEnum ? Enum.GetNames(FieldType) : Array.Empty<string>();
        public int EnumIndex
        {
            get => FieldType.IsEnum ? Array.IndexOf(Enum.GetValues(FieldType), Value) : -1;
            set
            {
                Array values = Enum.GetValues(FieldType);
                if (value >= 0 && value < values.Length)
                    Value = values.GetValue(value);
            }
        }

        // Visible fields of a nested struct field.
        public IReadOnlyList<SerializedProperty> Children
        {
            get
            {
                if (_children is null)
                {
                    _children = new List<SerializedProperty>();
                    Type liveType = _livePath[^1].FieldType;
                    if (liveType.IsValueType && !liveType.IsPrimitive && !liveType.IsEnum)
                        foreach ((FieldInfo live, FieldInfo editor) in SerializedObject.VisibleFields(liveType))
                            _children.Add(new SerializedProperty(_target, [.. _livePath, live], [.. _editorPath, editor]));
                }
                return _children;
            }
        }

        public bool HasChildren => Children.Count > 0;

        public T? GetAttribute<T>() where T : Attribute => FieldInfo.GetCustomAttribute<T>();

        private object? LiveValue
        {
            get
            {
                object? current = _target;
                foreach (FieldInfo step in _livePath)
                {
                    if (current is null)
                        return null;
                    current = step.GetValue(current);
                }
                return current;
            }
            set => Write(_target, 0, value);
        }

        private object Write(object container, int depth, object? value)
        {
            FieldInfo field = _livePath[depth];
            if (depth == _livePath.Length - 1)
            {
                field.SetValue(container, value);
                return container;
            }
            object? child = field.GetValue(container);
            if (child is null)
                return container;
            field.SetValue(container, Write(child, depth + 1, value));
            return container;
        }

        // "_moveSpeed" / "moveSpeed" / "MoveSpeed" -> "Move Speed".
        public static string Nicify(string name)
        {
            string trimmed = name.TrimStart('_');
            if (trimmed.Length > 2 && trimmed[0] == 'm' && trimmed[1] == '_')
                trimmed = trimmed[2..];
            StringBuilder builder = new(trimmed.Length + 4);
            for (int i = 0; i < trimmed.Length; i++)
            {
                char c = trimmed[i];
                if (i == 0)
                    builder.Append(char.ToUpperInvariant(c));
                else if (char.IsUpper(c) && !char.IsUpper(trimmed[i - 1]))
                    builder.Append(' ').Append(c);
                else
                    builder.Append(c);
            }
            return builder.ToString();
        }
    }
}
