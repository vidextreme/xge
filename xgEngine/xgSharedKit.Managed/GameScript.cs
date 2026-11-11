// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Reflection;

namespace xg
{
    // Marks a static, parameterless method in a game project's scripts to run once when the scripts load.
    [AttributeUsage(AttributeTargets.Method)]
    public sealed class GameInitAttribute : Attribute
    {
    }

    // Marks a static method taking (float deltaTime) to run every frame.
    [AttributeUsage(AttributeTargets.Method)]
    public sealed class GameUpdateAttribute : Attribute
    {
    }

    // Marks a static, parameterless method to run when the scripts unload.
    [AttributeUsage(AttributeTargets.Method)]
    public sealed class GameShutdownAttribute : Attribute
    {
    }

    // Runtime behind a game project's generated ScriptEntry: initializes the engine bindings for the module and calls
    // the [GameInit]/[GameUpdate]/[GameShutdown] methods found in the project's script assembly.
    public static class GameScriptHost
    {
        private static readonly List<Action> _init = new();
        private static readonly List<Action<float>> _update = new();
        private static readonly List<Action> _shutdown = new();

        public static Assembly? ScriptAssembly { get; private set; }

        public static bool Init(nint engine, uint moduleID, Assembly scripts)
        {
            ScriptRuntime.Initialize(engine, moduleID);
            ScriptAssembly = scripts;
            Collect(scripts);
            foreach (Action init in _init)
                Invoke(init);
            return true;
        }

        public static void Update(float deltaTime)
        {
            foreach (Action<float> update in _update)
            {
                try
                {
                    update(deltaTime);
                }
                catch (Exception e)
                {
                    Debug.LogException(e);
                }
            }
        }

        public static void Shutdown()
        {
            foreach (Action shutdown in _shutdown)
                Invoke(shutdown);
            _init.Clear();
            _update.Clear();
            _shutdown.Clear();
            ScriptRuntime.Shutdown();
        }

        private static void Collect(Assembly scripts)
        {
            Type[] types;
            try
            {
                types = scripts.GetTypes();
            }
            catch (ReflectionTypeLoadException e)
            {
                types = e.Types.Where(t => t is not null).ToArray()!;
            }

            foreach (Type type in types)
            {
                foreach (MethodInfo method in type.GetMethods(BindingFlags.Static | BindingFlags.Public | BindingFlags.NonPublic))
                {
                    if (method.IsDefined(typeof(GameInitAttribute)) && method.GetParameters().Length == 0)
                        _init.Add(method.CreateDelegate<Action>());
                    if (method.IsDefined(typeof(GameUpdateAttribute)) && IsUpdateSignature(method))
                        _update.Add(method.CreateDelegate<Action<float>>());
                    if (method.IsDefined(typeof(GameShutdownAttribute)) && method.GetParameters().Length == 0)
                        _shutdown.Add(method.CreateDelegate<Action>());
                }
            }
        }

        private static bool IsUpdateSignature(MethodInfo method)
        {
            ParameterInfo[] parameters = method.GetParameters();
            return parameters.Length == 1 && parameters[0].ParameterType == typeof(float);
        }

        private static void Invoke(Action action)
        {
            try
            {
                action();
            }
            catch (Exception e)
            {
                Debug.LogException(e);
            }
        }
    }
}
