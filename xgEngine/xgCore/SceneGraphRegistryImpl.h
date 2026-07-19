#pragma once

#include "xgSceneGraphRegistry.h"

namespace xg
{
    class SceneGraphRegistryImpl : public SceneGraphRegistry
    {
    public:
        void Register(const char* name, Factory factory) override;

        SceneGraph* Create(const char* name) override;

    private:
        std::unordered_map<std::string, Factory> _factories;
    };

}