#pragma once
#include "xgSceneGraphFactory.h"


namespace xg
{
    class SceneGraphFactoryImpl : public SceneGraphFactory
    {
    public:
        SceneGraphFactoryImpl() = default;
        ~SceneGraphFactoryImpl() override = default;

        void RegisterType(const char* typeName,
            SceneGraph* (*creator)(ActorRegistry&)) override;

        SceneGraph* Create(const char* typeName,
            ActorRegistry& registry) override;

    private:
        std::unordered_map<std::string,
            SceneGraph* (*)(ActorRegistry&)> _creators;
    };
}
