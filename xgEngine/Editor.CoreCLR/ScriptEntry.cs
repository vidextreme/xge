using System.Diagnostics;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using xg;
namespace Editor.CoreCLR
{
    public static class ScriptEntry
    {
        [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
        public static int Init(nint enginePtr)
        {
            ScriptEngine.Initialize(enginePtr);

            // Example: register a managed log callback
            ScriptEngine.AddLogCallback(OnLog);
            Console.WriteLine("wow");
            return 1;
        }

        [UnmanagedCallersOnly(EntryPoint = "Script_Update")]
        public static void Update(float dt) 
        {
            
        }

        [UnmanagedCallersOnly(EntryPoint = "Script_Shutdown")]
        public static void Shutdown() 
        {

        }

        private static void OnLog(MessageType type, IntPtr msgPtr)
        {
            string msg = Marshal.PtrToStringAnsi(msgPtr) ?? string.Empty;
           // Console.WriteLine($"[{type}] {msg} waasdadsds");
        }
    }
}
