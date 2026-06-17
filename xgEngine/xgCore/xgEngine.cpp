#include "pch.h"
#include "xgPathUtils.h"
#include "public/xgModules.h"
#include "public/xgEngine.h"
#include "public/xgScriptModule.h"
#include "public/xgScriptHost.h"
#include "public/xgScriptHost.h"

namespace xg
{
    // Internal-only storage (hidden behind void*)
    struct ModuleStorage
    {
        std::unordered_map<std::string, ScriptModule*> Modules;
    };

    struct HostStorage
    {
        std::vector<ScriptHost*> Hosts;
    };

    Engine::Engine()
    {
        MainWindow = NewWindow("xgEngine", 1280, 720);
        _moduleStorage = new ModuleStorage();
        _hostStorage = new HostStorage();
    }

    Engine::~Engine()
    {
        Shutdown();

        delete static_cast<ModuleStorage*>(_moduleStorage);
        delete static_cast<HostStorage*>(_hostStorage);

        _moduleStorage = nullptr;
        _hostStorage = nullptr;
    }

    bool Engine::Initialize(const EngineConfig& config)
    {
        //TODO look for a better place for this!
        ScriptCoreCLRDLL = "xgScriptCoreCLR.dll";
		ScriptNativeDLL = "xgScriptNative.dll";

        if (config.RendererModule)
            return SetRendererModule(config.RendererModule);


        return true;
    }

    bool Engine::SetRendererModule(const char* path)
    {
        if (Renderer)
        {
            DestroyRenderer(Renderer);
            Renderer = nullptr;
        }

        xg::RendererDLL = path;
        xg::RendererLib = nullptr;

        Renderer = CreateRenderer(*MainWindow);
        return Renderer != nullptr;
    }

    bool IsCoreCLRModule(const char* path)
    {
        return std::string(path).find(".CoreCLR.") != std::string::npos;
    }

    //
    // Backend selection logic
    //
    ScriptHost* Engine::GetOrCreateHostFor(const std::string& path)
    {
        std::string ext = xg::GetExtension(path.c_str());
		
        if (IsCoreCLRModule(path.c_str()))
        {
            return CreateScriptHostCoreCLR(path.c_str());
        }

        else //TODO temporary
        {
			return CreateScriptHostNative(path.c_str());
        }
        /*if (ext == ".NativeAOT.dll")
            return new HostNativeAOT();

        if (ext == ".lua")
            return new HostLua();

        if (ext == ".py")
            return new HostPython();*/

        return nullptr;
    }

    //
    // AddScriptModule(id, path, hostOverride)
    //
    ScriptHost* Engine::AddScriptModule(const char* id,
        const char* path,
        ScriptHost* hostOverride)
    {
        if (!id || !path)
            return nullptr;

        ScriptHost* host = hostOverride ? hostOverride
            : GetOrCreateHostFor(path);

        if (!host)
            return nullptr;

        if (!hostOverride)
        {
            auto* hosts = static_cast<HostStorage*>(_hostStorage);
            hosts->Hosts.push_back(host);
        }

        ScriptModule* module = host->LoadModule(id, path);
        if (!module || !module->IsValid())
            return nullptr;

        // 🔹 Call Init here
        if (!module->Init(this))
        {
            module->Shutdown();
            delete module;
            return nullptr;
        }

        auto* storage = static_cast<ModuleStorage*>(_moduleStorage);
        storage->Modules[id] = module;

        return host;
    }


    ScriptModule* Engine::GetScriptModule(const char* id)
    {
        if (!id)
            return nullptr;

        auto* storage = static_cast<ModuleStorage*>(_moduleStorage);
        auto it = storage->Modules.find(id);

        return (it != storage->Modules.end()) ? it->second : nullptr;
    }

    void Engine::RemoveScriptModule(const char* id)
    {
        if (!id)
            return;

        auto* storage = static_cast<ModuleStorage*>(_moduleStorage);
        auto it = storage->Modules.find(id);

        if (it != storage->Modules.end())
        {
            ScriptModule* module = it->second;
            if (module)
            {
                module->Shutdown();
                delete module;
            }

            storage->Modules.erase(it);
        }
    }

    void Engine::Run()
    {
        float dt = 0.016f;

        auto* storage = static_cast<ModuleStorage*>(_moduleStorage);

        while (!MainWindow->ShouldClose())
        {
            MainWindow->PollEvents();

            for (auto& pair : storage->Modules)
            {
                ScriptModule* module = pair.second;
                if (module && module->IsValid())
                {
                    module->Update(dt);
                }
            }

            if (Renderer)
                Renderer->Render();
        }
    }

    void Engine::Shutdown()
    {
        // Shutdown modules
        auto* storage = static_cast<ModuleStorage*>(_moduleStorage);

        for (auto& pair : storage->Modules)
        {
            ScriptModule* module = pair.second;
            if (module)
            {
                module->Shutdown();
                delete module;
            }
        }

        storage->Modules.clear();

        // Destroy hosts
        auto* hosts = static_cast<HostStorage*>(_hostStorage);
        for (ScriptHost* host : hosts->Hosts)
            delete host;

        hosts->Hosts.clear();

        // Renderer cleanup
        if (Renderer)
        {
            DestroyRenderer(Renderer);
            Renderer = nullptr;
        }

        xg::RendererLib = nullptr;
    }
}
