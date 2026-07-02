#include "pch.h"

#include "xgRoute.h"

namespace xg
{
    Route Route::Direct(const char* id)
    {
        Route r{};
        r.Type = RouteType::Direct;
        r.Id = id;
        r.DepthLimit = -1;
        r.Predicate = nullptr;
        return r;
    }

    Route Route::ChildrenOf(const char* id, int depthLimit)
    {
        Route r{};
        r.Type = RouteType::Children;
        r.Id = id;
        r.DepthLimit = depthLimit;
        r.Predicate = nullptr;
        return r;
    }

    Route Route::ParentOf(const char* id, int depthLimit)
    {
        Route r{};
        r.Type = RouteType::Parents;
        r.Id = id;
        r.DepthLimit = depthLimit;
        r.Predicate = nullptr;
        return r;
    }

    Route Route::SiblingsOf(const char* id)
    {
        Route r{};
        r.Type = RouteType::Siblings;
        r.Id = id;
        r.DepthLimit = -1;
        r.Predicate = nullptr;
        return r;
    }

    Route Route::Filtered(bool (*predicate)(ScriptModule*))
    {
        Route r{};
        r.Type = RouteType::Filtered;
        r.Id = nullptr;
        r.DepthLimit = -1;
        r.Predicate = predicate;
        return r;
    }
}
