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
            return 1;
        }
        [UnmanagedCallersOnly(EntryPoint = "xg:Update")]
        public static void Update(float dt)
        {

        }

        [UnmanagedCallersOnly(EntryPoint = "xg:Shutdown")]
        public static void Shutdown()
        {

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
