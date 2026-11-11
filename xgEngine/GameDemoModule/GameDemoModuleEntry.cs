using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using xg;

namespace GameDemoModule
{
    public static class GameDemoModuleEntry
    {
        [UnmanagedCallersOnly(EntryPoint = "XG_Init", CallConvs = new[] { typeof(CallConvCdecl) })]
        public static int Init(nint enginePtr, uint moduleID)
        {
            ScriptRuntime.Initialize(enginePtr, moduleID);

            // Example: register a managed log callback
            ScriptRuntime.AddLogCallback(OnLog);
            Console.WriteLine("wow");

            _enemy = Actor.Create("demo enemy").AddComponent<EnemyComponent>();
            return 1;
        }

        static EnemyComponent _enemy = null!;
        static bool _reported;

        [UnmanagedCallersOnly(EntryPoint = "XG_Update")]
        public static void Update(float dt)
        {
            long frames = ActorRegistry.FrameCount;
            if (!_reported && frames >= 60)
            {
                _reported = true;
                Debug.Log($"[Actors] NativeAOT: {ActorRegistry.Count} actor(s) after {frames} actor frames: EnemyComponent updated {_enemy.UpdateCount} times");
            }
        }

        [UnmanagedCallersOnly(EntryPoint = "XG_Shutdown")]
        public static void Shutdown()
        {
            ScriptRuntime.Shutdown();
        }

        private static void OnLog(MessageType type, IntPtr msgPtr)
        {
            string msg = Marshal.PtrToStringAnsi(msgPtr) ?? string.Empty;
            // Console.WriteLine($"[{type}] {msg} waasdadsds");
        }

        [UnmanagedCallersOnly(EntryPoint = "XG_OnMessage", CallConvs = new[] { typeof(CallConvCdecl) })]
        public static void OnMessage(nint typeName, nint payload, int payloadSize)
        {

        }
    }
}
