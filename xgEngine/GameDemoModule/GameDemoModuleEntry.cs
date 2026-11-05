using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using xg;

namespace GameDemoModule
{
    public static class GameDemoModuleEntry
    {
        [UnmanagedCallersOnly(EntryPoint = "xg:Init", CallConvs = new[] { typeof(CallConvCdecl) })]
        public static int Init(nint enginePtr, uint moduleID)
        {
            ScriptRuntime.Initialize(enginePtr, moduleID);

            // Example: register a managed log callback
            ScriptRuntime.AddLogCallback(OnLog);
            Console.WriteLine("wow");

            s_enemy = Actor.Create("demo enemy").AddComponent<EnemyComponent>();
            return 1;
        }

        static EnemyComponent s_enemy = null!;
        static bool s_reported;

        [UnmanagedCallersOnly(EntryPoint = "xg:Update")]
        public static void Update(float dt)
        {
            long frames = ActorRegistry.FrameCount;
            if (!s_reported && frames >= 60)
            {
                s_reported = true;
                Debug.Log($"[Actors] NativeAOT: {ActorRegistry.Count} actor(s) after {frames} actor frames: EnemyComponent updated {s_enemy.UpdateCount} times");
            }
        }

        [UnmanagedCallersOnly(EntryPoint = "xg:Shutdown")]
        public static void Shutdown()
        {
            ScriptRuntime.Shutdown();
        }

        private static void OnLog(MessageType type, IntPtr msgPtr)
        {
            string msg = Marshal.PtrToStringAnsi(msgPtr) ?? string.Empty;
            // Console.WriteLine($"[{type}] {msg} waasdadsds");
        }

        [UnmanagedCallersOnly(EntryPoint = "xg:OnMessage", CallConvs = new[] { typeof(CallConvCdecl) })]
        public static void OnMessage(nint typeName, nint payload, int payloadSize)
        {

        }
    }
}
