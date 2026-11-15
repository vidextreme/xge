// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Reflection;
using System.Runtime.Loader;

namespace xg.editor
{
    // A component type that Add Component can create. Key (TypeBridge.KeyOf) is how modules find their own copy.
    public sealed class ComponentType
    {
        internal ComponentType(Type type)
        {
            Type = type;
            Key = TypeBridge.KeyOf(type);
            Name = SerializedProperty.Nicify(type.Name);
        }

        public Type Type { get; }
        public string Key { get; }
        public string Name { get; }
    }

    // Component types of every CoreCLR module (they share this runtime, each in its own load context): public,
    // non-abstract, non-generic classes with a public parameterless constructor that have [ComponentIcon] or implement
    // IComponentUpdate. Shared assemblies loaded by several modules are listed once.
    public static class ComponentCatalog
    {
        private static List<ComponentType>? _types;

        public static IReadOnlyList<ComponentType> Types => _types ??= Build();

        // Rebuilds the list on next access (call after loading or reloading a module).
        public static void Invalidate() => _types = null;

        private static List<ComponentType> Build()
        {
            Dictionary<string, ComponentType> found = new();
            foreach (AssemblyLoadContext context in AssemblyLoadContext.All)
            {
                foreach (Assembly assembly in context.Assemblies)
                {
                    if (assembly.IsDynamic || IsFramework(assembly))
                        continue;
                    foreach (Type type in LoadableTypes(assembly))
                    {
                        if (!IsComponentType(type))
                            continue;
                        string key = TypeBridge.KeyOf(type);
                        if (!found.ContainsKey(key))
                            found[key] = new ComponentType(type);
                    }
                }
            }
            List<ComponentType> list = found.Values.ToList();
            list.Sort((a, b) => string.Compare(a.Name, b.Name, StringComparison.OrdinalIgnoreCase));
            return list;
        }

        public static bool IsComponentType(Type type)
        {
            if (!type.IsClass || type.IsAbstract || type.IsGenericTypeDefinition || type.ContainsGenericParameters || !type.IsPublic)
                return false;
            if (type.GetConstructor(Type.EmptyTypes) is null)
                return false;
            if (TypeBridge.GetAttributeData(type, typeof(ComponentIconAttribute)) is not null)
                return true;
            foreach (Type contract in type.GetInterfaces())
                if (TypeBridge.SameType(contract, typeof(IComponentUpdate)))
                    return true;
            return false;
        }

        private static bool IsFramework(Assembly assembly)
        {
            string name = assembly.GetName().Name ?? string.Empty;
            return name.StartsWith("System", StringComparison.Ordinal) || name.StartsWith("Microsoft", StringComparison.Ordinal)
                || name == "netstandard" || name == "mscorlib";
        }

        private static IEnumerable<Type> LoadableTypes(Assembly assembly)
        {
            try
            {
                return assembly.GetTypes();
            }
            catch (ReflectionTypeLoadException e)
            {
                return e.Types.Where(t => t is not null)!;
            }
            catch (Exception)
            {
                return Array.Empty<Type>();
            }
        }
    }
}
