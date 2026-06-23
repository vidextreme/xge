#include "pch.h"
#include "xgPathUtils.h"
#include "public/xgModules.h"
#include "public/xgEngine.h"
#include "public/xgScriptModule.h"
#include "public/xgScriptHost.h"
#include "ScriptTree.h"

namespace xg
{
    // Internal-only storage (hidden behind void*)
    struct ModuleStorage
    {
        std::unordered_map<std::string, ScriptModule*> Modules;
    };

    struct HostEntry
    {
        std::string Group;
        ScriptHost* Host;
    };

    struct HostStorage
    {
        std::vector<HostEntry> Hosts;
    };

    Engine::Engine()
    {
        MainWindow = NewWindow("xgEngine", 1280, 720);
        _moduleStorage = new ModuleStorage();
        _hostStorage = new HostStorage();

        _scriptTree = new ScriptTree();

        if (MainWindow)
        {
            MainWindow->SetEventQueue(&_queue);
            MainWindow->SetEventDispatcher(&_dispatcher);
        }
    }

    Engine::~Engine()
    {
        Shutdown();

        delete static_cast<ModuleStorage*>(_moduleStorage);
        delete static_cast<HostStorage*>(_hostStorage);

        delete _scriptTree;
        _scriptTree = nullptr;

        _moduleStorage = nullptr;
        _hostStorage = nullptr;
    }

    bool Engine::Initialize(const EngineConfig& config)
    {
        // TODO: look for a better place for this!
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

    static bool IsCoreCLRModule(const char* path)
    {
        return std::string(path).find(".CoreCLR.") != std::string::npos;
    }

    const char* Engine::GetDefaultGroupFor(const char* path)
    {
        if (!path)
            return "default";

        if (IsCoreCLRModule(path))
            return "coreclr";

        // TODO: refine as you add more backends
        return "native";
    }

    ScriptHost* Engine::FindHostInGroup(const char* group)
    {
        if (!group)
            return nullptr;

        auto* hosts = static_cast<HostStorage*>(_hostStorage);
        for (auto& entry : hosts->Hosts)
        {
            if (entry.Group == group)
                return entry.Host;
        }
        return nullptr;
    }

    ScriptHost* Engine::CreateHostFor(const char* path)
    {
        if (!path)
            return nullptr;

        if (IsCoreCLRModule(path))
        {
            return CreateScriptHostCoreCLR(path);
        }
        else
        {
            // TODO: temporary – native host for everything else
            return CreateScriptHostNative(path);
        }

        // Future:
        //  - NativeAOT
        //  - Lua
        //  - Python
        //  etc.
    }

    void Engine::RegisterHostInGroup(const char* group, ScriptHost* host)
    {
        auto* hosts = static_cast<HostStorage*>(_hostStorage);
        HostEntry entry;
        entry.Group = group ? group : "default";
        entry.Host = host;
        hosts->Hosts.push_back(std::move(entry));
    }

    //
    // AddScriptModule(id, path, group)
    //
    ScriptModule* Engine::AddScriptModule(const char* id,
        const char* path,
        const char* group)
    {
        if (!id || !path)
            return nullptr;

        const char* resolvedGroup = group ? group : GetDefaultGroupFor(path);

        // 1. Find or create host for this group
        ScriptHost* host = FindHostInGroup(resolvedGroup);
        if (!host)
        {
            host = CreateHostFor(path);
            if (!host)
            {
                xg::Log(xg::MessageType::Error,
                    "Failed to create script host for module: %s - %s",
                    id, path);
                return nullptr;
            }

            host->AddRef();
            RegisterHostInGroup(resolvedGroup, host);
        }

        // 2. Load module through host
        ScriptModule* module = host->LoadModule(id, path, group);
        if (!module || !module->IsValid())
        {
            xg::Log(xg::MessageType::Error,
                "Failed to load script module: %s - %s",
                id, path);
            return nullptr;
        }

        xg::Log(xg::MessageType::Info,
            "Loaded script module: %s - %s (group: %s)",
            id, path, resolvedGroup);

        // 3. Init module
        if (!module->Init(this))
        {
            module->Shutdown();
            delete module;
            return nullptr;
        }

        // 4. Register module
        auto* storage = static_cast<ModuleStorage*>(_moduleStorage);
        storage->Modules[id] = module;

        // 5. Add to ScriptTree
        _scriptTree->AddModule(module, ThreadDomain::MainThread);
        _scriptTree->DebugPrint();

        return module;
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
                _scriptTree->RemoveModule(module);

                ScriptHost* host = module->GetHost();
                XG_RELEASE_ONE(host);

                module->Shutdown();
                delete module;
            }

            storage->Modules.erase(it);
        }
    }

    void Engine::Run()
    {
        float dt = 0.016f;

        while (!MainWindow->ShouldClose())
        {
            MainWindow->PollEvents();

            _scriptTree->Update(dt);

            // if (Renderer)
            //     Renderer->Render();
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
                ScriptHost* host = module->GetHost();
                XG_RELEASE_ONE(host);

                module->Shutdown();
                delete module;
            }
        }

        storage->Modules.clear();

        // Destroy hosts
        auto* hosts = static_cast<HostStorage*>(_hostStorage);
        for (auto& entry : hosts->Hosts)
        {
            XG_SAFE_RELEASE(entry.Host);
        }
        hosts->Hosts.clear();

        // Renderer cleanup
        if (Renderer)
        {
            DestroyRenderer(Renderer);
            Renderer = nullptr;
        }

        xg::RendererLib = nullptr;
    }

    void Engine::AddLogCallback(LogCallback cb)
    {
        xg::AddLogCallback(cb);
    }

    void Engine::RemoveLogCallback(LogCallback cb)
    {
        xg::RemoveLogCallback(cb);
    }

}
