#pragma once
#include "xgCallback.h"
#include "xgScriptModule.h"

namespace xg
{
    class MemberCallbackBase : public Callback
    {
    public:
        virtual ~MemberCallbackBase() = default;

        virtual ScriptModule* GetOwnerModule() const = 0;
        virtual void InvokeDirect(const xgEvent& e) = 0;
    };

    template<typename T>
    class MemberCallback : public MemberCallbackBase
    {
    public:
        using Method = void(T::*)(const xgEvent&);

        MemberCallback(T* instance, Method method)
            : _instance(instance)
            , _method(method)
        {}

        ScriptModule* GetOwnerModule() const override
        {
            return static_cast<ScriptModule*>(_instance);
        }

        void Invoke(const xgEvent& e) override
        {
            _instance->GetScriptEngine()->Dispatch(this, e);
        }

        void InvokeDirect(const xgEvent& e) override
        {
            (_instance->*_method)(e);
        }

    private:
        T* _instance;
        Method _method;
    };
}
