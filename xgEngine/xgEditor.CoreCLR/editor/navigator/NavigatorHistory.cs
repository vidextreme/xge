// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

namespace xgEditor.CoreCLR
{
    public enum NavigatorMode
    {
        Directory,
        Tags,
        Search,
    }

    // One Navigator location: the mode, each mode's location and the kind filter (a bit per AssetKind; 0 = all files).
    public readonly record struct NavigatorState(NavigatorMode Mode, string Folder, string Tags, string Search, int Kinds)
    {
        public static string ModeName(NavigatorMode mode) => mode switch
        {
            NavigatorMode.Tags => "tags",
            NavigatorMode.Search => "search",
            _ => "dir",
        };

        public static NavigatorMode ParseMode(string? name) => name?.ToLowerInvariant() switch
        {
            "tags" => NavigatorMode.Tags,
            "search" => NavigatorMode.Search,
            _ => NavigatorMode.Directory,
        };
    }

    // Back/forward history of Navigator states plus the recently visited folders. Runtime only: never saved.
    public sealed class NavigatorHistory
    {
        private const int MaxStates = 200;
        private const int MaxRecentFolders = 32;

        private readonly List<NavigatorState> _states = new();
        private readonly List<string> _recentFolders = new();
        private int _index = -1;

        public bool CanBack => _index > 0;
        public bool CanForward => _index >= 0 && _index < _states.Count - 1;
        public IReadOnlyList<string> RecentFolders => _recentFolders;

        public void Push(NavigatorState state)
        {
            if (_index >= 0 && _states[_index] == state)
                return;
            if (_index < _states.Count - 1)
                _states.RemoveRange(_index + 1, _states.Count - _index - 1);
            _states.Add(state);
            if (_states.Count > MaxStates)
                _states.RemoveAt(0);
            _index = _states.Count - 1;
            if (state.Mode == NavigatorMode.Directory)
                Visit(state.Folder);
        }

        public NavigatorState? Back()
        {
            if (!CanBack)
                return null;
            return _states[--_index];
        }

        public NavigatorState? Forward()
        {
            if (!CanForward)
                return null;
            return _states[++_index];
        }

        private void Visit(string folder)
        {
            if (folder.Length == 0)
                return;
            _recentFolders.RemoveAll(f => string.Equals(f, folder, StringComparison.OrdinalIgnoreCase));
            _recentFolders.Insert(0, folder);
            if (_recentFolders.Count > MaxRecentFolders)
                _recentFolders.RemoveAt(_recentFolders.Count - 1);
        }
    }
}
