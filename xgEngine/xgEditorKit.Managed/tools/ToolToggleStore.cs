// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

namespace xg.editor
{
    // Storage of toggle states, keyed "<declaring type full name>.<method>". A future settings system replaces
    // the Tools subsystem's ToggleStore to persist them; the default keeps them for the session.
    public interface IToolToggleStore
    {
        bool TryGet(string key, out bool value);

        void Set(string key, bool value);
    }

    public sealed class MemoryToolToggleStore : IToolToggleStore
    {
        private readonly Dictionary<string, bool> _values = new();

        public bool TryGet(string key, out bool value) => _values.TryGetValue(key, out value);

        public void Set(string key, bool value) => _values[key] = value;
    }
}
