// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Reflection;

namespace xg.editor
{
    // Finds the PropertyDrawer for a field: a drawer for one of its PropertyAttributes first, then a drawer for its
    // type (or a base type, for drawers registered with useForChildren), then the struct drawer for nested structs.
    // Drawers registered later win, so a project's editor assembly can replace the built-in ones.
    public static class DrawerRegistry
    {
        private readonly struct Entry
        {
            public Entry(Type drawer, bool useForChildren)
            {
                Drawer = drawer;
                UseForChildren = useForChildren;
            }

            public Type Drawer { get; }
            public bool UseForChildren { get; }
        }

        private static readonly Dictionary<string, Entry> _drawers = new();
        private static readonly Dictionary<(Type, string, Type), PropertyDrawer> _instances = new();
        private static readonly HashSet<Assembly> _assemblies = new();

        public static int Count => _drawers.Count;

        static DrawerRegistry()
        {
            Register(typeof(DrawerRegistry).Assembly);
        }

        // Registers every PropertyDrawer with [CustomPropertyDrawer] in assembly. Returns how many drawers were added.
        public static int Register(Assembly assembly)
        {
            if (!_assemblies.Add(assembly))
                return 0;
            int added = 0;
            Type[] types;
            try
            {
                types = assembly.GetTypes();
            }
            catch (ReflectionTypeLoadException e)
            {
                types = e.Types.Where(t => t is not null).ToArray()!;
            }
            foreach (Type type in types)
            {
                if (type.IsAbstract || !typeof(PropertyDrawer).IsAssignableFrom(type))
                    continue;
                foreach (CustomPropertyDrawerAttribute target in type.GetCustomAttributes<CustomPropertyDrawerAttribute>())
                {
                    _drawers[TypeBridge.KeyOf(target.Type)] = new Entry(type, target.UseForChildren);
                    added++;
                }
            }
            _instances.Clear();
            return added;
        }

        public static PropertyDrawer GetDrawer(SerializedProperty property)
        {
            PropertyAttribute? attribute = null;
            Type? drawerType = null;
            foreach (object candidate in property.FieldInfo.GetCustomAttributes(true))
            {
                if (candidate is PropertyAttribute propertyAttribute && Find(candidate.GetType()) is Type found)
                {
                    attribute = propertyAttribute;
                    drawerType = found;
                    break;
                }
            }
            drawerType ??= Find(property.FieldType);
            drawerType ??= property.HasChildren ? typeof(StructDrawer) : typeof(ValueLabelDrawer);

            var key = (property.Target.GetType(), property.PropertyPath, drawerType);
            if (!_instances.TryGetValue(key, out PropertyDrawer? drawer))
            {
                drawer = (PropertyDrawer)Activator.CreateInstance(drawerType)!;
                drawer.Attribute = attribute;
                drawer.FieldInfo = property.FieldInfo;
                _instances[key] = drawer;
            }
            return drawer;
        }

        private static Type? Find(Type type)
        {
            if (_drawers.TryGetValue(TypeBridge.KeyOf(type), out Entry exact))
                return exact.Drawer;
            for (Type? t = type.BaseType; t is not null; t = t.BaseType)
                if (_drawers.TryGetValue(TypeBridge.KeyOf(t), out Entry entry) && entry.UseForChildren)
                    return entry.Drawer;
            return null;
        }
    }
}
