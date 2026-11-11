// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using xg;

namespace xgHub.CoreCLR
{
    // Project Hub: the only module hub.config loads. It runs the UI host itself (no editor kernel) and shows HubPanel.
    public static class ScriptEntry
    {
        [UnmanagedCallersOnly]
        public static int Init(nint enginePtr, uint moduleID)
        {
            ScriptRuntime.Initialize(enginePtr, moduleID);
            if (!UIHost.Start(saveLayout: false))
            {
                Debug.LogError("[Hub] UI host failed to start");
                return 0;
            }
            xg.imui.Panel.Open<HubPanel>();
            return 1;
        }

        [UnmanagedCallersOnly]
        public static void Update(float dt)
        {
            UIHost.Frame(dt);
        }

        [UnmanagedCallersOnly]
        public static void Shutdown()
        {
            xg.imui.ImUI.ShutdownAll();
            UIHost.Stop();
            ScriptRuntime.Shutdown();
        }

        [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
        public static void OnMessage(nint typeName, nint payload, int payloadSize)
        {
        }
    }
}
