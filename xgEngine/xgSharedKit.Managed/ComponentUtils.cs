// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Collections.Concurrent;
using System.Reflection;

namespace xg
{
    // =========================================================================
    // Blittable validator (recursive)
    // =========================================================================
    public static class BlittableValidator
    {
        private static readonly ConcurrentDictionary<Type, bool> _validated = new();

        public static void Validate<T>() where T : struct, IComponentData
        {
            var t = typeof(T);
            if (_validated.ContainsKey(t)) return;
            ValidateType(t);
            _validated[t] = true;
        }

        private static void ValidateType(Type t)
        {
            if (!t.IsValueType)
                Throw(t, "is not a value type");

            if (t.IsPrimitive || t.IsEnum)
                return;

            if (t.IsGenericType)
                Throw(t, "generic value types are not supported as component payloads");

            var fields = t.GetFields(BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic);
            foreach (var f in fields)
            {
                var ft = f.FieldType;
                if (ft.IsPrimitive || ft.IsEnum)
                    continue;

                if (ft.IsValueType && !ft.IsGenericType)
                {
                    // nested struct: recurse
                    ValidateType(ft);
                    continue;
                }

                Throw(t, $"contains non-blittable field '{f.Name}' of type {ft.FullName}");
            }
        }

        private static void Throw(Type t, string reason)
        {
            throw new InvalidOperationException($"Type {t.FullName} is not a valid blittable component payload: {reason}");
        }
    }

    // =========================================================================
    // Stride cache (Marshal.SizeOf for struct payloads)
    // =========================================================================
    static class ComponentStrideCache
    {
        private static readonly ConcurrentDictionary<Type, int> Cache = new();

        public static int GetOrCompute<T>() where T : struct
            => Cache.GetOrAdd(typeof(T), _ => Marshal.SizeOf<T>());

        public static int GetOrCompute(Type t)
            => Cache.GetOrAdd(t, _ => Marshal.SizeOf(t));
    }
}
