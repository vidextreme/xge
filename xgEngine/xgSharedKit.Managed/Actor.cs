// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

namespace xg
{
    public partial class Actor
    {
        public ActorID ID;

        private readonly List<object> _components = new();

        public bool IsValid { get; private set; } = true;

        private bool _adopted;

        private Transform? _transform;

        // True for a proxy of an actor another module created (see Adopt).
        public bool IsAdopted => _adopted;

        internal Actor()
        {
        }

        public static Actor Create()
        {
            Actor actor = new() { ID = ActorUtils.XG_CreateActor(ScriptRuntime._enginePtr) };
            ActorRegistry.Register(actor);
            return actor;
        }

        public static Actor Create(string actorName)
        {
            Actor actor = new() { ID = ActorUtils.XG_CreateActorWithName(ScriptRuntime._enginePtr, actorName) };
            ActorRegistry.Register(actor);
            return actor;
        }

        public IReadOnlyList<object> Components => _components;

        public T AddComponent<T>() where T : class, new() => (T)AddComponent(new T());

        // Adds a new component of type (a class with a public parameterless constructor, or Transform).
        public object AddComponent(Type type)
        {
            ArgumentNullException.ThrowIfNull(type);
            if (type == typeof(Transform))
                return AddTransform();
            return AddComponent(Activator.CreateInstance(type) ?? throw new ArgumentException($"Cannot create {type}.", nameof(type)));
        }

        // Attaches an existing object (runtime attachment). An object has one owner at a time: remove it from its current
        // actor first. IComponent objects get OnAttached.
        public object AddComponent(object component)
        {
            ArgumentNullException.ThrowIfNull(component);
            ThrowIfDestroyed();
            if (component is Transform)
                return AddTransform();
            Actor? owner = ActorRegistry.GetOwner(component);
            if (owner is not null)
                throw new InvalidOperationException(ReferenceEquals(owner, this)
                    ? $"{component.GetType().Name} is already attached to actor {(ulong)ID}."
                    : $"{component.GetType().Name} is attached to actor {(ulong)owner.ID}; remove it there first.");
            AddRequired(component.GetType());
            _components.Add(component);
            ActorRegistry.SetOwner(component, this);
            if (component is IComponent attachable)
                attachable.OnAttached(this);
            if (component is IComponentUpdate updatable)
                updatable.Attach();
            return component;
        }

        // The actor's native transform, or null when it has none.
        public Transform? Transform
        {
            get
            {
                if (!IsValid)
                    return null;
                if (_transform is not null && _transform.IsValid)
                    return _transform;
                _transform = Transform.Exists(ID) ? new Transform(ID) : null;
                return _transform;
            }
        }

        // Adds an identity transform, or returns the existing one.
        public Transform AddTransform()
        {
            ThrowIfDestroyed();
            if (!Transform.Exists(ID) && !Transform.Add(ID))
                throw new InvalidOperationException($"Cannot add a Transform to actor {(ulong)ID}.");
            return _transform ??= new Transform(ID);
        }

        public bool RemoveTransform()
        {
            ThrowIfDestroyed();
            _transform = null;
            return Transform.Remove(ID);
        }

        private void AddRequired(Type type)
        {
            foreach (object attribute in type.GetCustomAttributes(typeof(RequireComponentAttribute), true))
            {
                Type required = ((RequireComponentAttribute)attribute).Type;
                if (required == typeof(Transform))
                    AddTransform();
                else if (!HasComponent(required))
                    AddComponent(required);
            }
        }

        private bool HasComponent(Type type)
        {
            foreach (object component in _components)
                if (type.IsInstanceOfType(component))
                    return true;
            return false;
        }

        // A proxy for an actor another module created, so this module can hold components on it. Destroying the proxy
        // only drops this module's view of the actor.
        internal static Actor Adopt(ActorID id)
        {
            Actor actor = new() { ID = id, _adopted = true };
            ActorRegistry.Register(actor);
            return actor;
        }

        // An actor this module owned before a script reload (adopted: it was a proxy of another module's actor then).
        internal static Actor Reattach(ActorID id, bool adopted)
        {
            Actor actor = new() { ID = id, _adopted = adopted };
            ActorRegistry.Register(actor);
            return actor;
        }

        public T? GetComponent<T>() where T : class
        {
            if (typeof(T) == typeof(Transform))
                return Transform as T;
            foreach (object component in _components)
                if (component is T match)
                    return match;
            return null;
        }

        internal bool RemoveComponent(object component)
        {
            int index = _components.IndexOf(component);
            if (index < 0)
                return false;
            DetachComponent(component);
            _components.RemoveAt(index);
            return true;
        }

        public bool RemoveComponent<T>() where T : class
        {
            if (typeof(T) == typeof(Transform))
                return RemoveTransform();
            for (int i = 0; i < _components.Count; i++)
            {
                if (_components[i] is T)
                {
                    DetachComponent(_components[i]);
                    _components.RemoveAt(i);
                    return true;
                }
            }
            return false;
        }

        public void Destroy()
        {
            if (!IsValid)
                return;
            Release();
            ActorRegistry.Unregister(this);
            if (!_adopted)
                ActorUtils.XG_DestroyActor(ScriptRuntime._enginePtr, ID);
        }

        public string Name
        {
            get { ThrowIfDestroyed(); return ActorUtils.GetActorName(ID); }
            set { ThrowIfDestroyed(); ActorUtils.SetActorName(ID, value); }
        }

        // Parent in the actor hierarchy. The getter is null when there is no parent or the parent belongs to another
        // module (use ActorTree.GetParent for ids across modules). Setting null makes the actor a root.
        public Actor? Parent
        {
            get
            {
                ThrowIfDestroyed();
                ActorID? parent = ActorTree.GetParent(ID);
                return parent is ActorID id && ActorRegistry.TryGet(id, out Actor? actor) ? actor : null;
            }
            set
            {
                ThrowIfDestroyed();
                if (!SetParent(value?.ID))
                    throw new InvalidOperationException($"Actor {(ulong)ID} cannot be parented to {(value is null ? "null" : ((ulong)value.ID).ToString())}.");
            }
        }

        public bool SetParent(ActorID? parent)
        {
            ThrowIfDestroyed();
            return ActorTree.SetParent(ID, parent);
        }

        internal void Release()
        {
            foreach (object component in _components)
                DetachComponent(component);
            _components.Clear();
            _transform = null;
            IsValid = false;
        }

        private static void DetachComponent(object component)
        {
            if (component is IComponentUpdate updatable)
                updatable.Detach();
            if (component is IComponent attachable)
                attachable.OnDetached();
            ActorRegistry.ClearOwner(component);
        }

        private void ThrowIfDestroyed()
        {
            if (!IsValid)
                throw new ObjectDisposedException(nameof(Actor), $"Actor {(ulong)ID} has been destroyed");
        }

        internal partial class ActorUtils
        {
            [LibraryImport("xgGameKernel", EntryPoint = "XG_CreateActor")]
            internal static partial ulong XG_CreateActor(nint engine);

            [LibraryImport("xgGameKernel", EntryPoint = "XG_CreateActorWithName", StringMarshalling = StringMarshalling.Utf8)]
            internal static partial ulong XG_CreateActorWithName(nint engine, string actorName);

            internal static string GetActorName(ActorID id)
            {
                nint ptr = XG_GetActorName(ScriptRuntime._enginePtr, id);
                return Marshal.PtrToStringUTF8(ptr) ?? string.Empty;
            }

            internal static void SetActorName(ActorID id, string newName)
            {
                XG_SetActorName(ScriptRuntime._enginePtr, id, newName);
            }

            [LibraryImport("xgGameKernel", EntryPoint = "XG_GetActorName")]
            internal static partial nint XG_GetActorName(nint engine, ulong actorID);

            [LibraryImport("xgGameKernel", EntryPoint = "XG_SetActorName", StringMarshalling = StringMarshalling.Utf8)]
            internal static partial nint XG_SetActorName(nint engine, ulong actorID, string newName);

            [LibraryImport("xgGameKernel", EntryPoint = "XG_DestroyActor")]
            internal static partial void XG_DestroyActor(nint engine, ulong actorID);
        }
    }
}
