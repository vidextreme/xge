// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once

#include "xgScriptModule.h"
#include "public/xgModuleFunctions.h"

#include <string>

namespace xg
{
    // A script module driven through entry points the caller loaded itself (XG_AddFunctionModule); it has no host and
    // no library of its own. Owns copies of its id and group.
    class ScriptModuleFunctions : public ScriptModule
    {
    public:
        ScriptModuleFunctions(std::string id, uint32_t moduleId, std::string group, const XG_ModuleFunctions& functions);

        bool Init(ScriptEngine* engine) override;
        void Update(float dt) override;
        void Shutdown() override;
        bool IsValid() const override;
        void OnMessage(const ScriptMessage& msg) override;
        SceneGraph* GetSceneGraph() override;

    private:
        static const char* GroupOrNull(const std::string& group) { return group.empty() ? nullptr : group.c_str(); }

        std::string _idText;
        std::string _groupText;
        XG_ModuleFunctions _functions;
        bool _initialized = false;
    };
}
