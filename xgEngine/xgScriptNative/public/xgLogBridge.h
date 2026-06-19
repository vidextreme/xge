#pragma once
#include "xgBase.h"
#include "xgLog.h"
#include "xgModules.h"
namespace xg
{
    // Optional: convenience wrapper for managed binding
    XG_MODULE_EXPORT void InitializeManagedLogBridge();
    XG_MODULE_EXPORT void ShutdownManagedLogBridge();
    XG_MODULE_EXPORT void AddManagedLogCallback(LogCallback cb);
    XG_MODULE_EXPORT void RemoveManagedLogCallback(LogCallback cb);

}
