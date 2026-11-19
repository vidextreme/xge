// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg;

namespace xgEditor.CoreCLR
{
    // XG_SCRIPT_TEST_RELOAD hook: before each reload, sets Health.Current on "SampleGame Player" to a new value and
    // remembers it; after the reload, logs whether the actor id and the value came through.
    internal static class ScriptReloadTest
    {
        private static ActorID _actor = ActorID.InvalidID;
        private static float _expected;
        private static int _round;

        public static void BeforeReload()
        {
            _actor = FindActor("SampleGame Player");
            object? health = FindComponent(_actor, "Health");
            if (health is null)
            {
                Debug.Log("[Debug] Reload test: no SampleGame Player with Health");
                return;
            }
            _expected = 10.0f + ++_round;
            health.GetType().GetField("Current")?.SetValue(health, _expected);
            Debug.Log($"[Debug] Reload test {_round}: actor {(ulong)_actor} Health.Current set to {_expected}");
        }

        public static void AfterReload()
        {
            if (_actor == ActorID.InvalidID)
                return;
            object? health = FindComponent(_actor, "Health");
            object? value = health?.GetType().GetField("Current")?.GetValue(health);
            Debug.Log($"[Debug] Reload test {_round}: actor {(ulong)_actor} {(health is null ? "lost its Health" : $"Health.Current = {value} (expected {_expected})")}, " +
                $"{System.Runtime.Loader.AssemblyLoadContext.All.Count(c => c.Name == ProjectService.ProjectModuleId)} game.project context(s) alive");
        }

        private static ActorID FindActor(string name)
        {
            List<ActorID> pending = new();
            ActorTree.GetRoots(pending);
            List<ActorID> children = new();
            for (int i = 0; i < pending.Count; i++)
            {
                if (ActorTree.GetName(pending[i]) == name)
                    return pending[i];
                ActorTree.GetChildren(pending[i], children);
                pending.AddRange(children);
            }
            return ActorID.InvalidID;
        }

        private static object? FindComponent(ActorID actor, string typeName)
        {
            if (actor == ActorID.InvalidID)
                return null;
            List<object> components = new();
            ActorTree.GetComponents(actor, components);
            return components.FirstOrDefault(c => c.GetType().Name == typeName);
        }
    }
}
