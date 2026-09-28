#pragma once
#include "xgScriptModule.h"

namespace xg
{
	class GameKernelModule : public ScriptModule
	{
	public:
		GameKernelModule(const char* id, int moduleID, ScriptHost* host, const char* group);
		~GameKernelModule() override;
		bool Init(ScriptEngine* engine) override;
		void Update(float dt) override;
		void Shutdown() override;

		bool IsValid() const override { return _isValid; }

		SceneGraph* GetSceneGraph() override;
		void OnMessage(const ScriptMessage& msg) override;
	private:
		ScriptEngine* _engine = nullptr;
		ScriptModule* _gameModule = nullptr;
		bool _isValid = true;
	};
}

