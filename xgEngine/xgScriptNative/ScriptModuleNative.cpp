#include "pch.h"
#include "ScriptModuleNative.h"
#include "xgScriptEngine.h"

namespace xg
{

        ScriptModuleNative::ScriptModuleNative(
			const char* id,
            void* lib,
            InitFunc init,
            UpdateFunc update,
            ShutdownFunc shutdown)
        : ScriptModule(id),
            _lib(lib)
            , _init(init)
            , _update(update)
            , _shutdown(shutdown)
        {
            _isValid =
                (_lib != nullptr) &&
                (_init != nullptr) &&
                (_update != nullptr) &&
                (_shutdown != nullptr);
        }

    ScriptModuleNative::~ScriptModuleNative()
    {
        if (_initialized)
            Shutdown();

        if (_lib)
        {
            xg::UnloadModule(_lib);
            _lib = nullptr;
        }
    }

    bool ScriptModuleNative::Init(ScriptEngine* engine)
    {
        if (!_isValid || !_init || _initialized)
            return false;

        if (!_init(engine))
        {
            _isValid = false;
            return false;
        }

        _initialized = true;
        return true;
    }

    void ScriptModuleNative::Update(float dt)
    {
        if (_isValid && _initialized && _update)
            _update(dt);
    }

    void ScriptModuleNative::Shutdown()
    {
        if (_isValid && _initialized && _shutdown)
            _shutdown();

        _initialized = false;
    }

    bool ScriptModuleNative::IsValid() const
    {
        return _isValid;
    }
}
