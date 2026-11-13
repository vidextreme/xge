// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Reflection;
using System.Runtime.Loader;

namespace xg.editor
{
    // Every CoreCLR script module runs in its own AssemblyLoadContext, so a live game object's types are not the
    // editor's types even when they come from the same assembly file. The bridge maps a type to the editor's copy by
    // assembly and full name, and converts values between the two copies (fields copied by name).
    public static class TypeBridge
    {
        private static readonly Dictionary<Type, Type> _toEditor = new();

        public static AssemblyLoadContext EditorContext =>
            AssemblyLoadContext.GetLoadContext(typeof(TypeBridge).Assembly) ?? AssemblyLoadContext.Default;

        // A stable key for a type across load contexts.
        public static string KeyOf(Type type) => $"{type.FullName}, {type.Assembly.GetName().Name}";

        public static bool SameType(Type a, Type b) => a == b || KeyOf(a) == KeyOf(b);

        // The editor's copy of type, or type itself when the editor has no copy of its assembly.
        public static Type ToEditor(Type type)
        {
            if (type.Assembly.IsDynamic || AssemblyLoadContext.GetLoadContext(type.Assembly) == EditorContext)
                return type;
            if (_toEditor.TryGetValue(type, out Type? cached))
                return cached;

            Type result = type;
            string? name = type.Assembly.GetName().Name;
            foreach (Assembly assembly in EditorContext.Assemblies.Concat(AssemblyLoadContext.Default.Assemblies))
            {
                if (assembly.GetName().Name != name || type.FullName is null)
                    continue;
                Type? match = assembly.GetType(type.FullName, false);
                if (match is not null)
                {
                    result = match;
                    break;
                }
            }
            _toEditor[type] = result;
            return result;
        }

        public static bool Derives(Type type, Type baseType)
        {
            for (Type? t = type; t is not null; t = t.BaseType)
                if (SameType(t, baseType))
                    return true;
            return false;
        }

        public static bool HasAttribute(MemberInfo member, Type attributeType)
        {
            foreach (CustomAttributeData data in member.CustomAttributes)
                if (Derives(data.AttributeType, attributeType))
                    return true;
            return false;
        }

        // Converts value to target, which may be the same type from another load context.
        public static object? Convert(object? value, Type target)
        {
            if (value is null || target.IsInstanceOfType(value))
                return value;
            Type source = value.GetType();
            if (target.IsEnum)
                return Enum.ToObject(target, System.Convert.ChangeType(value, Enum.GetUnderlyingType(target)));
            if (target.IsPrimitive || target == typeof(decimal))
                return System.Convert.ChangeType(value, target);
            if (target.IsValueType && SameType(source, target))
            {
                object result = Activator.CreateInstance(target)!;
                foreach (FieldInfo field in target.GetFields(BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic))
                {
                    FieldInfo? from = source.GetField(field.Name, BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic);
                    if (from is not null)
                        field.SetValue(result, Convert(from.GetValue(value), field.FieldType));
                }
                return result;
            }
            return value;
        }
    }
}
