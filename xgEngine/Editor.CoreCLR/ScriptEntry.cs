using System.Diagnostics;
using System.Runtime.InteropServices;

namespace xgEditor
{
    public static class ScriptEntry
    {
        [UnmanagedCallersOnly(EntryPoint = "Script_Init")]
        public static void Init()
        {
            try
            {
                //System.Diagnostics.Debugger.Launch();
                Debugger.Break();
            }
            catch { }

            Console.WriteLine("wow");
            //return true;
        }

        [UnmanagedCallersOnly(EntryPoint = "Script_Update")]
        public static void Update(float dt) 
        {
            
        }

        [UnmanagedCallersOnly(EntryPoint = "Script_Shutdown")]
        public static void Shutdown() { }
    }
}
