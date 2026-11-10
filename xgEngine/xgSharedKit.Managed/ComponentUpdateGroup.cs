// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

namespace xg
{
    internal interface IComponentUpdateGroup
    {
        int Count { get; }
        void Update(float deltaTime);
        void FlushRemovals();
        void Clear();
    }

    internal sealed unsafe class ComponentUpdateGroup<T> : IComponentUpdateGroup
        where T : IComponentUpdate<T>
    {
        public static readonly ComponentUpdateGroup<T> Instance = new();

        private static readonly delegate* managed<T, float, void> _update = &T.OnUpdate;

        private readonly List<T?> _components = new();
        private int _removed;
        private bool _registered;

        public int Count => _components.Count - _removed;

        public void Add(T component)
        {
            if (!_registered)
            {
                ActorRegistry.AddGroup(this);
                _registered = true;
            }
            _components.Add(component);
        }

        public void Remove(T component)
        {
            for (int i = 0; i < _components.Count; i++)
            {
                if (ReferenceEquals(_components[i], component))
                {
                    _components[i] = default;
                    _removed++;
                    return;
                }
            }
        }

        public void Update(float deltaTime)
        {
            int count = _components.Count;
            for (int i = 0; i < count; i++)
            {
                T? component = _components[i];
                if (component is not null)
                    _update(component, deltaTime);
            }
        }

        public void FlushRemovals()
        {
            if (_removed == 0)
                return;
            _components.RemoveAll(c => c is null);
            _removed = 0;
        }

        public void Clear()
        {
            _components.Clear();
            _removed = 0;
        }
    }
}
