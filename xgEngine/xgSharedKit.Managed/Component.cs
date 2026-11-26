// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Reflection;

namespace xg
{
    // Optional for anything attached to an actor: Actor.AddComponent calls OnAttached, removal calls OnDetached.
    // Any object can be attached; implementing this only adds the callbacks.
    public interface IComponent
    {
        void OnAttached(Actor actor) { }
        void OnDetached() { }
    }

    // Optional convenience base for components: keeps the owning actor and reaches its transform and siblings.
    public abstract class Component : IComponent
    {
        public Actor Actor { get; private set; } = null!;
        public ActorID ActorID => Actor.ID;
        public Transform? Transform => Actor?.Transform;
        public bool IsAttached => Actor is { IsValid: true };

        public T? GetComponent<T>() where T : class => Actor?.GetComponent<T>();

        void IComponent.OnAttached(Actor actor)
        {
            Actor = actor;
            AttachInternal(actor);
            OnAttached();
        }

        void IComponent.OnDetached()
        {
            OnDetached();
            if (Actor is not null)
                DetachInternal(Actor);
            Actor = null!;
        }

        internal virtual void AttachInternal(Actor actor) { }
        internal virtual void DetachInternal(Actor actor) { }

        protected virtual void OnAttached() { }
        protected virtual void OnDetached() { }
    }

    // Actor.AddComponent adds these component types first (Transform is added through the TransformSystem).
    [AttributeUsage(AttributeTargets.Class, AllowMultiple = true, Inherited = true)]
    public sealed class RequireComponentAttribute : Attribute
    {
        public Type Type { get; }
        public RequireComponentAttribute(Type type) => Type = type;
    }

    // Earlier names, so data saved before a rename still loads: a component type's earlier full names
    // ("xg.ObjectDriverComponent"; ComponentTypeKeys.Current maps old type keys), a field's earlier name ("Object") or an
    // enum or struct's earlier full name (ReloadState.ApplyFields accepts values saved under them).
    [AttributeUsage(AttributeTargets.Class | AttributeTargets.Struct | AttributeTargets.Enum | AttributeTargets.Field, AllowMultiple = true, Inherited = false)]
    public sealed class FormerlyNamedAttribute : Attribute
    {
        public string FullName { get; }
        public FormerlyNamedAttribute(string fullName) => FullName = fullName;
    }

    // Component type keys ("Full.Name, Assembly") across renames.
    public static class ComponentTypeKeys
    {
        // The key itself when its type exists, else the key of a type in that assembly marked [FormerlyNamed] with its
        // full name; the key unchanged when neither is found.
        public static string Current(string key)
        {
            int comma = key.LastIndexOf(',');
            if (comma <= 0)
                return key;
            string fullName = key[..comma].Trim();
            string assemblyName = key[(comma + 1)..].Trim();
            foreach (Assembly assembly in System.Runtime.Loader.AssemblyLoadContext.All.SelectMany(c => c.Assemblies))
            {
                if (assembly.GetName().Name != assemblyName)
                    continue;
                if (assembly.GetType(fullName, false) is not null)
                    return key;
                if (Renamed(assembly, fullName) is Type renamed)
                    return $"{renamed.FullName}, {assemblyName}";
            }
            return key;
        }

        // The type of this assembly that was called fullName before, or null.
        public static Type? Renamed(Assembly assembly, string fullName)
        {
            try
            {
                return assembly.GetTypes().FirstOrDefault(t => t.GetCustomAttributes<FormerlyNamedAttribute>(false).Any(a => a.FullName == fullName));
            }
            catch (ReflectionTypeLoadException)
            {
                return null;
            }
        }
    }

    public interface IComponentUpdate : IComponent
    {
        void Setup();
        void Update(float deltaTime);

        void Attach();
        void Detach();
    }

    // A blittable struct stored natively in its own pool (see ComponentDataSet, ComponentData).
    public interface IComponentData { }
    public unsafe interface IComponentUpdate<T> : IComponentUpdate
        where T : IComponentUpdate<T>
    {
        protected static delegate* managed<T, float, void> UpdateFunc { get; set; }
        static abstract void OnUpdate(T component, float deltaTime);
        void IComponentUpdate.Setup()
        {
            UpdateFunc = &T.OnUpdate;
        }

        void IComponentUpdate.Update(float deltaTime)
        {
            UpdateFunc((T)this, deltaTime);
        }

        void IComponentUpdate.Attach()
        {
            ComponentUpdateGroup<T>.Instance.Add((T)this);
        }

        void IComponentUpdate.Detach()
        {
            ComponentUpdateGroup<T>.Instance.Remove((T)this);
        }
    }
}
