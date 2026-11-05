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
            Actor actor = new() { ID = ActorUtils.XGCreateActor(ScriptRuntime._enginePtr) };
            ActorRegistry.Register(actor);
            return actor;
        }

        public static Actor Create(string actorName)
        {
            Actor actor = new() { ID = ActorUtils.XGCreateActorWithName(ScriptRuntime._enginePtr, actorName) };
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
            ActorUtils.XGDestroyActor(ScriptRuntime._enginePtr, ID);
        }

        public string Name
        {
            get { ThrowIfDestroyed(); return ActorUtils.GetActorName(ID); }
            set { ThrowIfDestroyed(); ActorUtils.SetActorName(ID, value); }
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
            [LibraryImport("xgGameKernel", EntryPoint = "XGCreateActor")]
            internal static partial ulong XGCreateActor(nint engine);

            [LibraryImport("xgGameKernel", EntryPoint = "XGCreateActorWithName", StringMarshalling = StringMarshalling.Utf8)]
            internal static partial ulong XGCreateActorWithName(nint engine, string actorName);

            internal static string GetActorName(ActorID id)
            {
                nint ptr = XGGetActorName(ScriptRuntime._enginePtr, id);
                return Marshal.PtrToStringUTF8(ptr);
            }

            internal static void SetActorName(ActorID id, string newName)
            {
                XGSetActorName(ScriptRuntime._enginePtr, id, newName);
            }

            [LibraryImport("xgGameKernel", EntryPoint = "XGGetActorName")]
            internal static partial nint XGGetActorName(nint engine, ulong actorID);

            [LibraryImport("xgGameKernel", EntryPoint = "XGSetActorName", StringMarshalling = StringMarshalling.Utf8)]
            internal static partial nint XGSetActorName(nint engine, ulong actorID, string newName);

            [LibraryImport("xgGameKernel", EntryPoint = "XGDestroyActor")]
            internal static partial void XGDestroyActor(nint engine, ulong actorID);
        }
    }
}
