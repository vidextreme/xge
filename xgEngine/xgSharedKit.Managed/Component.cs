// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

namespace xg
{
    public interface IComponent
    {
        static abstract void Update<T>(T component) where T : IComponent;
    }

    public interface IComponentUpdate
    {
        void Setup();
        void Update(float deltaTime);
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
