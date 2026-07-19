#include "pch.h"
#include "xgSceneGraph.h"
#include "SceneGraphRegistryImpl.h"

void xg::SceneGraphRegistryImpl::Register(const char* name, Factory factory)
{
    _factories[name] = factory;
}

xg::SceneGraph* xg::SceneGraphRegistryImpl::Create(const char* name)
{
    auto it = _factories.find(name);
    if (it == _factories.end())
        return nullptr;

    return it->second(); // call factory
}
