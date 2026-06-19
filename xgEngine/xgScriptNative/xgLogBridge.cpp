#include "pch.h"
#include "xgLogBridge.h"
#include "xgLog.h"
#include "xgModules.h"

namespace xg
{
    static LogCallback g_ManagedCallback = nullptr;

    void InitializeManagedLogBridge()
    {
        // Managed side will call AddLogCallback, but we keep a reference
        // so we can remove it cleanly on shutdown.
        // Nothing else needed here.
    }

    void ShutdownManagedLogBridge()
    {
        if (g_ManagedCallback)
        {
           // RemoveLogCallback(g_ManagedCallback);
            g_ManagedCallback = nullptr;
        }
    }

    // Called from managed side
    void AddManagedLogCallback(LogCallback cb)
    {
        g_ManagedCallback = cb;
        //AddLogCallback(cb);
    }

    void RemoveManagedLogCallback(LogCallback cb)
    {
        //RemoveLogCallback(cb);
        if (g_ManagedCallback == cb)
            g_ManagedCallback = nullptr;
    }

}
