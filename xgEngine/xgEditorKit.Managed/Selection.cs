// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

namespace xg.editor
{
    // The editor selection: actors (ActorID) and assets (AssetRef), set by the Hierarchy, the Navigator and tools. It
    // only keeps valid items (Validate drops destroyed actors and deleted assets). There are no selection events: every
    // change is sent straight to the Tools subsystem, which updates which tools are usable. Current is the selection as
    // a ToolSelection, which holds the tool drivers: each change builds the next one (drivers of items still selected
    // carry over) and releases the previous one, so deselected items lose their drivers unless a view retains them.
    public static class Selection
    {
        private static object[] _objects = Array.Empty<object>();
        private static ToolSelection _current = new(Array.Empty<object>());
        private static ActorID[] _actors = Array.Empty<ActorID>();
        private static AssetRef[] _assets = Array.Empty<AssetRef>();
        private static int _validateFrame;

        // Current items in selection order (the last one is the active item). Never modify the returned array.
        public static object[] Objects => _objects;
        public static int Count => _objects.Length;
        public static int Version { get; private set; }
        // The current selection with its tool drivers. Retain it to keep it (and its drivers) past the next change.
        public static ToolSelection Current => _current;
        public static object? ActiveObject => _objects.Length == 0 ? null : _objects[^1];
        public static ActorID ActiveActor => _actors.Length == 0 ? ActorID.InvalidID : _actors[^1];
        public static IReadOnlyList<ActorID> Actors => _actors;
        public static IReadOnlyList<AssetRef> Assets => _assets;

        public static bool Contains(object item) => Array.IndexOf(_objects, item) >= 0;

        public static void Set(params object[] items)
        {
            List<object> next = new(items.Length);
            foreach (object item in items)
                if (IsValid(item) && !next.Contains(item))
                    next.Add(item);
            Apply(next.ToArray());
        }

        public static void Add(object item)
        {
            if (!IsValid(item) || Contains(item))
                return;
            Apply([.. _objects, item]);
        }

        public static void Remove(object item)
        {
            if (Contains(item))
                Apply(_objects.Where(o => !o.Equals(item)).ToArray());
        }

        public static void Toggle(object item)
        {
            if (Contains(item))
                Remove(item);
            else
                Add(item);
        }

        public static void Clear()
        {
            if (_objects.Length > 0)
                Apply(Array.Empty<object>());
        }

        // Rewrites selected asset paths, e.g. after a rename or move (map returns the path unchanged when unaffected).
        public static void RemapAssets(Func<string, string> map)
        {
            bool changed = false;
            object[] next = new object[_objects.Length];
            for (int i = 0; i < _objects.Length; i++)
            {
                object item = _objects[i];
                if (item is AssetRef asset)
                {
                    string path = map(asset.Path);
                    if (!string.Equals(path, asset.Path, StringComparison.Ordinal))
                    {
                        item = new AssetRef(path);
                        changed = true;
                    }
                }
                next[i] = item;
            }
            if (changed)
                Apply(next);
        }

        // Drops actors that no longer exist (every call) and assets whose file is gone (every 30th call).
        public static void Validate()
        {
            if (_objects.Length == 0)
                return;
            bool checkAssets = ++_validateFrame % 30 == 0;
            object[]? kept = null;
            for (int i = 0; i < _objects.Length; i++)
            {
                object item = _objects[i];
                bool valid = item switch
                {
                    ActorID actor => ActorTree.GetName(actor).Length > 0,
                    AssetRef asset => !checkAssets || ScriptRuntime.ProjectPath.Length == 0 || File.Exists(asset.FullPath) || Directory.Exists(asset.FullPath),
                    _ => true,
                };
                if (!valid)
                    kept ??= _objects.Take(i).ToArray();
                else if (kept is not null)
                    kept = [.. kept, item];
            }
            if (kept is not null)
                Apply(kept);
        }

        // Starts the current selection over with no drivers (after the scripts or tools behind them were reloaded).
        public static void Refresh()
        {
            ToolSelection previous = _current;
            _current = new ToolSelection(_objects);
            Version++;
            Subsystem.Get<Tools>()?.NotifySelection(_current);
            previous.Release();
        }

        private static bool IsValid(object? item) => item switch
        {
            null => false,
            ActorID actor => actor != ActorID.InvalidID && ActorTree.GetName(actor).Length > 0,
            AssetRef asset => !string.IsNullOrEmpty(asset.Path),
            _ => true,
        };

        private static void Apply(object[] next)
        {
            if (next.SequenceEqual(_objects))
                return;
            _objects = next;
            _actors = next.OfType<ActorID>().ToArray();
            _assets = next.OfType<AssetRef>().ToArray();
            Version++;
            ToolSelection previous = _current;
            _current = previous.Next(next);
            Subsystem.Get<Tools>()?.NotifySelection(_current);
            previous.Release();
        }
    }
}
