// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

namespace xg.editor
{
    // An editor icon chosen for an actor or a Hierarchy group: a built-in Icon name ("Camera") or a project-relative image
    // path ("Assets/Icons/enemy.png"), and an optional "#RRGGBB" tint for built-in icons.
    public readonly record struct EditorIcon(string Name, string? Color);

    // Editor-only per-actor state: hidden (skipped by the Scene view only) and locked (selectable and browsable, but tools,
    // context menu tools and the Inspector won't edit it). Both are inherited: an actor is effectively hidden or locked
    // when it, an ancestor, or the container of its root (a Hierarchy folder or the scene, supplied by Containers) is.
    // Children keep their own flags. Scenes save the flags of their members; runtime actors keep them for the session.
    public static class ActorEditorState
    {
        private static readonly HashSet<ulong> _hidden = new();
        private static readonly HashSet<ulong> _locked = new();
        private static readonly Dictionary<ulong, EditorIcon> _icons = new();
        private static bool _showHidden;

        // Effective state of the container a root actor sits in (its folder chain and scene), or (false, false).
        public static Func<ActorID, (bool Hidden, bool Locked)>? Containers { get; set; }

        // Bumped on every change (flags, containers, the override).
        public static int Version { get; private set; }

        // Raised after the flags of these actors changed (empty for container or override changes).
        public static event Action<IReadOnlyList<ActorID>>? Changed;

        // The Scene view draws hidden actors anyway; every flag stays as it is.
        public static bool ShowHidden
        {
            get => _showHidden;
            set
            {
                if (_showHidden == value)
                    return;
                _showHidden = value;
                Bump(Array.Empty<ActorID>());
            }
        }

        public static bool AnyHidden => _hidden.Count > 0;

        public static bool IsHidden(ActorID actor) => _hidden.Contains(actor.Value);

        public static bool IsLocked(ActorID actor) => _locked.Contains(actor.Value);

        public static bool IsHiddenInHierarchy(ActorID actor) => Inherited(actor, _hidden, true);

        public static bool IsLockedInHierarchy(ActorID actor) => Inherited(actor, _locked, false);

        // Hidden or locked by an ancestor or container, not by its own flag.
        public static bool IsHiddenByParent(ActorID actor) => ParentState(actor, _hidden, true);

        public static bool IsLockedByParent(ActorID actor) => ParentState(actor, _locked, false);

        // True when the actor or any descendant is effectively locked (deleting it would delete a locked actor).
        public static bool IsLockedInSubtree(ActorID actor)
        {
            if (IsLockedInHierarchy(actor))
                return true;
            if (_locked.Count == 0)
                return false;
            List<ActorID> children = new();
            ActorTree.GetChildren(actor, children);
            foreach (ActorID child in children)
                if (IsLockedInSubtree(child))
                    return true;
            return false;
        }

        public static bool AnyLockedInHierarchy(IEnumerable<object> items)
        {
            foreach (object item in items)
                if (item is ActorID actor && IsLockedInHierarchy(actor))
                    return true;
            return false;
        }

        // The icon chosen for the actor, or null for the default.
        public static EditorIcon? GetIcon(ActorID actor) => _icons.TryGetValue(actor.Value, out EditorIcon icon) ? icon : null;

        // Sets (or with null clears) the icon of these actors.
        public static void SetIcon(IReadOnlyList<ActorID> actors, EditorIcon? icon)
        {
            List<ActorID> changed = new();
            foreach (ActorID actor in actors)
            {
                if (actor.Value == 0 || GetIcon(actor) == icon)
                    continue;
                if (icon is { } value)
                    _icons[actor.Value] = value;
                else
                    _icons.Remove(actor.Value);
                changed.Add(actor);
            }
            if (changed.Count > 0)
                Bump(changed);
        }

        // Sets the icon while loading a scene (no Changed event).
        public static void RestoreIcon(ActorID actor, EditorIcon? icon)
        {
            if (icon is { } value)
                _icons[actor.Value] = value;
            else if (!_icons.Remove(actor.Value))
                return;
            Version++;
        }

        public static void SetHidden(IReadOnlyList<ActorID> actors, bool hidden) => Set(_hidden, actors, hidden);

        public static void SetLocked(IReadOnlyList<ActorID> actors, bool locked) => Set(_locked, actors, locked);

        // Sets flags while loading a scene (no Changed event, so the scene isn't touched).
        public static void Restore(ActorID actor, bool hidden, bool locked)
        {
            if (hidden ? _hidden.Add(actor.Value) : _hidden.Remove(actor.Value))
                Version++;
            if (locked ? _locked.Add(actor.Value) : _locked.Remove(actor.Value))
                Version++;
        }

        // Folder or scene state changed (the container resolver answers differently now).
        public static void ContainersChanged() => Bump(Array.Empty<ActorID>());

        // Drops the flags of actors that no longer exist.
        public static void Prune()
        {
            int removed = _hidden.RemoveWhere(id => ActorTree.GetName(new ActorID(id)).Length == 0)
                + _locked.RemoveWhere(id => ActorTree.GetName(new ActorID(id)).Length == 0);
            foreach (ulong id in _icons.Keys.Where(id => ActorTree.GetName(new ActorID(id)).Length == 0).ToArray())
                removed += _icons.Remove(id) ? 1 : 0;
            if (removed > 0)
                Version++;
        }

        // Every effectively hidden actor under these roots, for the render systems.
        public static void CollectHidden(IEnumerable<ActorID> roots, List<ulong> result)
        {
            List<ActorID> children = new();
            foreach (ActorID root in roots)
                Collect(root, Containers?.Invoke(root).Hidden ?? false, result, children);
        }

        private static void Collect(ActorID actor, bool inherited, List<ulong> result, List<ActorID> scratch)
        {
            bool hidden = inherited || _hidden.Contains(actor.Value);
            if (hidden)
                result.Add(actor.Value);
            scratch.Clear();
            ActorTree.GetChildren(actor, scratch);
            if (scratch.Count == 0)
                return;
            ActorID[] children = scratch.ToArray();
            foreach (ActorID child in children)
                Collect(child, hidden, result, scratch);
        }

        private static void Set(HashSet<ulong> set, IReadOnlyList<ActorID> actors, bool on)
        {
            List<ActorID> changed = new();
            foreach (ActorID actor in actors)
                if (actor.Value != 0 && (on ? set.Add(actor.Value) : set.Remove(actor.Value)))
                    changed.Add(actor);
            if (changed.Count > 0)
                Bump(changed);
        }

        private static void Bump(IReadOnlyList<ActorID> changed)
        {
            Version++;
            Subsystem.Get<Tools>()?.Invalidate();
            Changed?.Invoke(changed);
        }

        private static bool Inherited(ActorID actor, HashSet<ulong> set, bool hidden) =>
            set.Contains(actor.Value) || ParentState(actor, set, hidden);

        private static bool ParentState(ActorID actor, HashSet<ulong> set, bool hidden)
        {
            ActorID current = actor;
            for (int guard = 0; guard < 256; guard++)
            {
                ActorID? parent = ActorTree.GetParent(current);
                if (parent is not { } p || p.Value == 0)
                    break;
                if (set.Contains(p.Value))
                    return true;
                current = p;
            }
            if (Containers is null)
                return false;
            (bool containerHidden, bool containerLocked) = Containers(current);
            return hidden ? containerHidden : containerLocked;
        }
    }
}
