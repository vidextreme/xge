// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

namespace xg
{
    public partial class Actor
    {
        public ActorID ID;

        private readonly List<object> _components = new();

        public bool IsValid { get; private set; } = true;

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

        public T AddComponent<T>() where T : class, new()
        {
            ThrowIfDestroyed();
            T component = new();
            _components.Add(component);
            ActorRegistry.SetOwner(component, this);
            if (component is IComponentUpdate updatable)
                updatable.Attach();
            return component;
        }

        public T? GetComponent<T>() where T : class
        {
            foreach (object component in _components)
                if (component is T match)
                    return match;
            return null;
        }

        public bool RemoveComponent<T>() where T : class
        {
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
            IsValid = false;
        }

        private static void DetachComponent(object component)
        {
            if (component is IComponentUpdate updatable)
                updatable.Detach();
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
