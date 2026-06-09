using System.Runtime.InteropServices;

namespace Editor.CoreCLR
{
    public static class ScriptEntry
    {
        [UnmanagedCallersOnly(EntryPoint = "Script_Init")]
        public static bool Init(nint enginePtr) => true;

        [UnmanagedCallersOnly(EntryPoint = "Script_Update")]
        public static void Update(float dt) { }

        [UnmanagedCallersOnly(EntryPoint = "Script_Shutdown")]
        public static void Shutdown() { }
    }
}
