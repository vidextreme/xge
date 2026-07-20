#include "pch.h"
#include "SceneGraphFactoryImpl.h"

namespace xg
{
    void SceneGraphFactoryImpl::RegisterType(
        const char* typeName,
        SceneGraph* (*creator)(ActorRegistry&))
    {
        _creators[typeName] = creator;
    }

    SceneGraph* SceneGraphFactoryImpl::Create(
        const char* typeName,
        ActorRegistry& registry)
    {
        auto it = _creators.find(typeName);
        if (it == _creators.end())
            return nullptr;

        return it->second(registry);
    }
}