// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

namespace xg.editor
{
    // Per-view state of single-instance tools: tabs, folds, search text, typed text. A view (an Inspector window) owns
    // one and passes it, scoped per tool, through the tool's context. State<T> keeps one object per type; Scope gives a
    // child view; Dispose disposes every IDisposable state, including those of the scopes.
    public sealed class ToolView : IDisposable
    {
        private readonly Dictionary<Type, object> _states = new();
        private Dictionary<object, ToolView>? _scopes;

        public T State<T>() where T : class, new()
        {
            if (_states.TryGetValue(typeof(T), out object? state))
                return (T)state;
            T created = new();
            _states[typeof(T)] = created;
            return created;
        }

        public bool TryGetState<T>(out T? state) where T : class
        {
            bool found = _states.TryGetValue(typeof(T), out object? value);
            state = value as T;
            return found;
        }

        public ToolView Scope(object key)
        {
            _scopes ??= new Dictionary<object, ToolView>();
            if (!_scopes.TryGetValue(key, out ToolView? scope))
            {
                scope = new ToolView();
                _scopes[key] = scope;
            }
            return scope;
        }

        public void Dispose()
        {
            foreach (object state in _states.Values)
            {
                if (state is not IDisposable disposable)
                    continue;
                try
                {
                    disposable.Dispose();
                }
                catch (Exception e)
                {
                    Debug.LogException(e);
                }
            }
            _states.Clear();
            if (_scopes is null)
                return;
            foreach (ToolView scope in _scopes.Values)
                scope.Dispose();
            _scopes.Clear();
        }
    }
}
