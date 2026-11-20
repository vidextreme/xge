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
