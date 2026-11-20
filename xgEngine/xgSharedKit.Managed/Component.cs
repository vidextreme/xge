// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

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
            OnAttached();
        }

        void IComponent.OnDetached()
        {
            OnDetached();
            Actor = null!;
        }

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

    public interface IComponentUpdate : IComponent
    {
        void Setup();
        void Update(float deltaTime);

        void Attach();
        void Detach();
    }

    public interface IComponentData { }
    public interface IEditScope : IDisposable { }

    public struct EditScope : IEditScope
    {
        private ComponentData _data;
        public EditScope(ComponentData data)
        {
            _data = data;
        }
        public void Dispose()
        {
            _data.Commit();
        }
    }
}
