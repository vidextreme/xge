// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

namespace xg.editor
{
    // One selection and the tool drivers made for it. Tools stay single instances; the data they work on lives in
    // drivers created on demand (DriverFor) and keyed by tool and item (ITool.DriverKey), so the same tool serves any
    // number of selections. Next builds the following selection and carries over the drivers whose item is still
    // selected; the rest are disposed once nothing holds them. Selections are reference counted: the editor selection
    // holds the current one, and a view (a locked Inspector) retains the one it shows. It has no UI or editor
    // dependency, so batch processes can build one with new ToolSelection(objects).
    public sealed class ToolSelection
    {
        private sealed class DriverSlot
        {
            public required ITool Tool;
            public required object Key;
            public IToolDriver? Driver;
            public int References;
        }

        private readonly List<DriverSlot> _slots = new();
        private readonly Dictionary<ITool, ToolUsability> _usability = new(ReferenceEqualityComparer.Instance);
        private int _references = 1;

        // Called when a tool callback or a driver throws. Defaults to Debug.LogException.
        public static Action<Exception>? ErrorHandler { get; set; }

        // The new selection is held once by its creator; Release that reference when done.
        public ToolSelection(object[] objects) => Objects = objects;

        // Items in selection order; the last one is the active item. Never modify the array.
        public object[] Objects { get; }
        public int Count => Objects.Length;
        public object? ActiveObject => Objects.Length == 0 ? null : Objects[^1];
        public bool IsReleased => _references <= 0;

        public bool Contains(object item) => Array.IndexOf(Objects, item) >= 0;

        public IEnumerable<T> Items<T>() => Objects.OfType<T>();

        // The last selected item of type T (the active one of that type), or null.
        public object? Last<T>()
        {
            for (int i = Objects.Length - 1; i >= 0; --i)
                if (Objects[i] is T)
                    return Objects[i];
            return null;
        }

        // ------------------------------------------------------------
        // Lifetime
        // ------------------------------------------------------------
        public ToolSelection Retain()
        {
            if (_references > 0)
                ++_references;
            return this;
        }

        // Drops one reference. The last one releases every driver slot, disposing drivers no other selection holds.
        public void Release()
        {
            if (_references <= 0 || --_references > 0)
                return;
            foreach (DriverSlot slot in _slots)
                ReleaseSlot(slot);
            _slots.Clear();
            _usability.Clear();
        }

        // The selection that follows this one: drivers whose key is still among the objects move to it (shared until
        // this selection is released), every other driver stays here and goes when this selection is released.
        public ToolSelection Next(object[] objects)
        {
            ToolSelection next = new(objects);
            foreach (DriverSlot slot in _slots)
            {
                if (!Carries(slot, objects))
                    continue;
                ++slot.References;
                next._slots.Add(slot);
            }
            return next;
        }

        // ------------------------------------------------------------
        // Tools and drivers
        // ------------------------------------------------------------
        // The tool's answer to OnSelectItems for these objects, asked once and cached.
        public ToolUsability Usability(ITool tool)
        {
            if (_usability.TryGetValue(tool, out ToolUsability usability))
                return usability;
            usability = ToolUsability.NotApplicable;
            try
            {
                usability = tool.OnSelectItems(Objects);
            }
            catch (Exception e)
            {
                Report(e);
            }
            if (_references > 0)
                _usability[tool] = usability;
            return usability;
        }

        // Asks every tool again on the next Usability call (e.g. after state the tools read has changed).
        public void InvalidateUsability() => _usability.Clear();

        // The tool's driver for its current key, created on first use. Null when the tool has no driver for this
        // selection (no key, CreateDriver returned null or threw) or the selection was released.
        public IToolDriver? DriverFor(ITool tool)
        {
            if (_references <= 0)
                return null;
            object? key = null;
            try
            {
                key = tool.DriverKey(this);
            }
            catch (Exception e)
            {
                Report(e);
            }
            if (key is null)
                return null;
            foreach (DriverSlot existing in _slots)
                if (ReferenceEquals(existing.Tool, tool) && existing.Key.Equals(key))
                    return existing.Driver;
            DriverSlot slot = new() { Tool = tool, Key = key, References = 1 };
            try
            {
                slot.Driver = tool.CreateDriver(this);
            }
            catch (Exception e)
            {
                Report(e);
            }
            _slots.Add(slot);
            return slot.Driver;
        }

        public T? Driver<T>(ITool tool) where T : class, IToolDriver => DriverFor(tool) as T;

        // Every driver created so far for this selection, including the carried-over ones.
        public IEnumerable<IToolDriver> Drivers => _slots.Where(s => s.Driver is not null).Select(s => s.Driver!);

        public bool HasPendingChanges => _slots.Any(s => PendingOf(s));

        // Drivers with pending changes that a selection of these objects wouldn't carry over: what deselecting would
        // lose.
        public List<IToolDriver> PendingOnDeselect(object[] objects) =>
            _slots.Where(s => PendingOf(s) && !Carries(s, objects)).Select(s => s.Driver!).ToList();

        private bool Carries(DriverSlot slot, object[] objects) =>
            slot.Driver is not null && !ReferenceEquals(slot.Key, this) && Array.IndexOf(objects, slot.Key) >= 0;

        private static bool PendingOf(DriverSlot slot)
        {
            if (slot.Driver is null)
                return false;
            try
            {
                return slot.Driver.HasPendingChanges;
            }
            catch (Exception e)
            {
                Report(e);
                return false;
            }
        }

        private static void ReleaseSlot(DriverSlot slot)
        {
            if (--slot.References > 0 || slot.Driver is null)
                return;
            IToolDriver driver = slot.Driver;
            slot.Driver = null;
            try
            {
                driver.Dispose();
            }
            catch (Exception e)
            {
                Report(e);
            }
        }

        private static void Report(Exception e)
        {
            if (ErrorHandler is { } handler)
                handler(e);
            else
                Debug.LogException(e);
        }
    }
}
