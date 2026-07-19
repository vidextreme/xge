// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once

#include "xgScriptModule.h"

namespace xg
{
    class ScriptModuleNull : public ScriptModule
    {
    public:
        ScriptModuleNull(const char* id,
            uint32_t moduleID,
            const char* group = "logic");

        ~ScriptModuleNull() override;

        bool Init(ScriptEngine* engine) override;
        void Update(float dt) override;
        void Shutdown() override;
        bool IsValid() const override;

        void OnMessage(const ScriptMessage& msg) override;
        SceneGraph* GetSceneGraph() override;
    private:
        bool _isValid = true;
    };
}
