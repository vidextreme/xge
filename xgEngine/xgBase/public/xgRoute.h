#pragma once
#include <cstdint>
#include "xgBase.h"

namespace xg
{
    class ScriptModule;

    XG_ENUM(inherit=byte)
    enum class RouteType : uint8_t
    {
        Direct,
        Children,
        Parents,
        Siblings,
        Filtered
    };

    struct Route
    {
        RouteType   Type;
        const char* Id;          // Direct / Children / Parents / Siblings
        int         DepthLimit;  // Children / Parents (-1 = infinite)
        bool      (*Predicate)(ScriptModule*); // Filtered

        // Factory functions (implemented in cpp)
        static Route Direct(const char* id);
        static Route ChildrenOf(const char* id, int depthLimit = -1);
        static Route ParentOf(const char* id, int depthLimit = -1);
        static Route SiblingsOf(const char* id);
        static Route Filtered(bool (*predicate)(ScriptModule*));
    };
}
