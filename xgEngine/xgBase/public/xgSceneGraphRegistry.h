#pragma once

namespace xg
{
    class SceneGraph;
    class SceneGraphRegistry
    {
    public:
        using Factory = SceneGraph* (*)();

        virtual ~SceneGraphRegistry() = default;

        virtual void Register(const char* name, Factory factory) = 0;
        virtual SceneGraph* Create(const char* name) = 0;
    };

}