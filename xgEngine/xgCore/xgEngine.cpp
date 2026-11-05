// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"
#include "xgPathUtils.h"
#include "public/xgModules.h"
#include "public/xgEngine.h"
#include "public/xgScriptModule.h"
#include "public/xgScriptHost.h"
#include "MessengerImpl.h"

#include "ScriptTree.h"
#include "ScriptHostNull.h"

#include "xgTypeRegistry.h"
#include "xgCodecRegistry.h"

#include "EventDispatcherImpl.h"
#include "EventQueueImpl.h"
#include "CodecRegistryImpl.h"
#include "DynamicObjectImpl.h"
#include "ActorRegistryImpl.h"
#include "LoggerImpl.h"
#include "SceneGraphFactoryImpl.h"

#include "TreeSceneGraph.h"
#include "CompositeSceneGraph.h"

#include "CodecJson.h"
#include "CodecBinary.h"

namespace xg
{    
    // Internal-only storage (hidden behind void*)
    struct ModuleStorage
    {
        std::unordered_map<std::string, ScriptModule*> Modules;
    };

    struct HostEntry
    {
        std::string       Group;
        ScriptBackendType Backend;
        ScriptHost* Host;
    };

    struct HostStorage
    {
        std::vector<HostEntry> Hosts;
    };

    Engine::Engine()
    {
        _logger = new LoggerImpl();
        xg::SetLogger(_logger);

        //MainWindow = NewWindow("xgEngine", 1280, 720);
        _moduleStorage = new ModuleStorage();
        _hostStorage = new HostStorage();

        _scriptTree = new ScriptTree();
        //_codec = new JsonMessageCodec();
        //_messenger = new MessengerImpl(_scriptTree);
        _rootModule = AddScriptModule("root", nullptr, nullptr, "core");
		_rootNode = _scriptTree->FindNode(_rootModule);
        //auto ha = MessengerImpl::SuperTypeID;
		RegisterSystem(_rootModule, new MessengerImpl(_scriptTree));
        RegisterSystem(_rootModule, new ActorRegistryImpl());
        
        auto eventDispatcher = new EventDispatcherImpl();
        RegisterSystem(_rootModule, eventDispatcher);

		auto sceneGraphFactory = new SceneGraphFactoryImpl();
        RegisterSystem(_rootModule, sceneGraphFactory);

		auto queue = new EventQueueImpl();
        RegisterSystem(_rootModule, queue);

        sceneGraphFactory->RegisterType("tree",
            [](ActorRegistry& reg) -> SceneGraph*
            {
                return new TreeSceneGraph(reg);
            });

        sceneGraphFactory->RegisterType("compset",
            [](ActorRegistry& reg) -> SceneGraph*
            {
                return new CompositeSceneGraph(reg);
            });

		_typeRegistry = CreateTypeRegistry();

		_nativeCodecRegistry = new CodecRegistryImpl();
		_coreclrCodecRegistry = new CodecRegistryImpl();

        RegisterTypes_xgEngineDef(_typeRegistry.get());
    }

    Engine::~Engine()
    {
        Shutdown();

        delete static_cast<ModuleStorage*>(_moduleStorage);
        delete static_cast<HostStorage*>(_hostStorage);

        XG_DELETE(_scriptTree)
		XG_DELETE(_nativeCodecRegistry)
		XG_DELETE(_coreclrCodecRegistry)

        _moduleStorage = nullptr;
        _hostStorage = nullptr;

        if (xg::GetLogger() == _logger)
            xg::SetLogger(nullptr);
        XG_DELETE(_logger)
    }

    bool Engine::Initialize(const EngineConfig& config)
    {
        _config = config;

        // TODO: look for a better place for this!
        xg::ScriptCoreCLRDLL = "xgScriptCoreCLR.dll";
        xg::ScriptNativeDLL = "xgScriptNative.dll";

        if (MainWindow)
        {
            MainWindow->SetEventQueue(GetSystem<EventQueue>());
            MainWindow->SetEventDispatcher(GetSystem<EventDispatcher>());
        }

        CodecRegistry* registry = GetCodecRegistry(ScriptBackendType::CoreCLR);
        PayloadMode mode = GetPayloadMode();
        if (mode == PayloadMode::JSON)
        {
            // Register generic JSON codec
            registry->RegisterEncoder("*", Encode_JSON_Generic);
            registry->RegisterDecoder("*", Decode_JSON_Generic);
        }
        else if (mode == PayloadMode::BINARY)
        {
            // Register generic binary codec
            registry->RegisterEncoder("*", Encode_Binary_Generic);
            registry->RegisterDecoder("*", Decode_Binary_Generic);
        }
        
        if (config.RendererModule && !SetRendererModule(config.RendererModule))
            return false;

        return true;
    }

    bool Engine::SetRendererModule(const char* path)
    {
        Renderer.reset();

        xg::RendererDLL = path;
        xg::RendererLib = nullptr;

        Renderer = CreateRenderer(*MainWindow);
        return Renderer.valid();
    }

    static bool IsCoreCLRModule(const char* path)
    {
        return std::string(path).find(".CoreCLR.") != std::string::npos;
    }

    static ScriptBackendType DetectBackend(const char* path)
    {
        if(path == nullptr)
			return ScriptBackendType::Null;

        if (IsCoreCLRModule(path))
            return ScriptBackendType::CoreCLR;
        
        // TODO: refine as you add more backends
        return ScriptBackendType::Native;
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

    ScriptHost* Engine::FindHostInGroupForBackend(const char* group,
        ScriptBackendType backend)
    {
        if (!group)
            return nullptr;

        auto* hosts = static_cast<HostStorage*>(_hostStorage);
        for (auto& entry : hosts->Hosts)
        {
            if (entry.Group == group && entry.Backend == backend)
                return entry.Host;
        }
        return nullptr;
    }

    ScriptHost* Engine::CreateHostFor(ScriptBackendType backend,
        const char* path)
    {
        if (!path)
            return new ScriptHostNull(this);

        switch (backend)
        {
        case ScriptBackendType::CoreCLR:
            return CreateScriptHostCoreCLR(this, path);

        case ScriptBackendType::Native:
        default:
            return CreateScriptHostNative(this, path);
			//TODO
        }
    }

    void Engine::RegisterHostInGroup(const char* group,
        ScriptBackendType backend,
        ScriptHost* host)
    {
        auto* hosts = static_cast<HostStorage*>(_hostStorage);
        HostEntry entry;
        entry.Group = group ? group : "default";
        entry.Backend = backend;
        entry.Host = host;
        hosts->Hosts.push_back(std::move(entry));
    }

    System* Engine::GetSystemImpl(TypeID id, ScriptModule* module)
    {
        if (module == nullptr)
            return _rootNode->GetSystem(id);

        System* system = nullptr;
        ScriptNode* node = _scriptTree->FindNode(module);
        if (node)
        {
            system = node->GetSystem(id);
            if(system == nullptr)
                system = node->GetParent()->GetSystem(id);
        }
        return system;
    }

    System* Engine::GetSystemImpl(TypeID id, const char* moduleID)
    {
		ScriptModule* module = GetScriptModule(moduleID);       

        return GetSystemImpl(id, module);
    }

    void Engine::RegisterSystemImpl(ScriptModule* module, TypeID id, System* system)
    {
        if (module == nullptr)
        {
            _rootNode->RegisterSystem(id, system);
            return;
        }

        ScriptNode* node = _scriptTree->FindNode(module);
        if (node)
        {
            node->RegisterSystem(id, system);
        }
    }

    ScriptModule* Engine::AddScriptModule(const char* id,
        const char* path,
        ScriptModule* parent,
        const char* group)
    {
        if (!id)
            return nullptr;

        if(parent == nullptr)
			parent = _rootModule;

        const char* resolvedGroup = group ? group : GetDefaultGroupFor(path);
        ScriptBackendType backend = DetectBackend(path);

        // 1. Find or create host for this group + backend
        ScriptHost* host = FindHostInGroupForBackend(resolvedGroup, backend);
        if (!host)
        {
            host = CreateHostFor(backend, path);
            if (!host)
            {
                xg::Log(xg::MessageType::Error,
                    "Failed to create script host for module: %s - %s",
                    id, path);
                return nullptr;
            }

            RegisterHostInGroup(resolvedGroup, backend, host);
        }

        // 2. Load module through host
        uint32_t newModuleID = ++_nextModuleId;
        ScriptModule* module = host->LoadModule(id, newModuleID, path, resolvedGroup);
        if (!module || !module->IsValid())
        {
            xg::Log(xg::MessageType::Error,
                "Failed to load script module: %s - %s",
                id, path);
            return nullptr;
        }

        host->AddRef();

        xg::Log(xg::MessageType::Info,
            "Loaded script module: %s - %s (group: %s)",
            id, path, resolvedGroup);

        // 3. Register module BEFORE Init so parent lookup works
        auto* storage = static_cast<ModuleStorage*>(_moduleStorage);
        storage->Modules[id] = module;

        // 4. Insert into ScriptTree BEFORE Init
        ScriptNode* parentNode = nullptr;
        if (parent)
            parentNode = _scriptTree->FindNode(parent);

        ScriptNode* node = _scriptTree->AddModule(module, parent, ThreadDomain::MainThread);

        if (!node)
        {
            xg::Log(xg::MessageType::Error,
                "ScriptTree failed to attach module: %s", id);
            storage->Modules.erase(id);
            delete module;
            XG_RELEASE_ONE(host);
            return nullptr;
        }

        // 5. Init module AFTER it is in the tree
        if (!module->Initialize(this))
        {
            _scriptTree->RemoveModule(module);
            storage->Modules.erase(id);

            module->Shutdown();
            delete module;
            XG_RELEASE_ONE(host);
            return nullptr;
        }

        _scriptTree->ForEachNodeRecursive(node, [](ScriptNode* n)
            {
                n->ForEachSystem([](System* system)
                    {
                        system->Initialize();
                    });
            });
        //_scriptTree->DebugPrint();
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
                storage->Modules.erase(it);
                _scriptTree->RemoveModule(module);

                ScriptHost* host = module->GetHost();
                module->Shutdown();
                delete module;
                XG_RELEASE_ONE(host);
                return;
            }

            storage->Modules.erase(it);
        }
    }

    ScriptModule* Engine::GetScriptModule(uint32_t moduleID)
    {
        ScriptModule* module = nullptr;
        auto* storage = static_cast<ModuleStorage*>(_moduleStorage);
        for (auto& [id, content] : storage->Modules)
        {
            if (content->GetModuleId() == moduleID)
            {
                module = content;
                break;
            }
        }
        return module;
    }

    void Engine::Run()
    {
        static constexpr float MaxDeltaTime = 0.25f;

        using Clock = std::chrono::steady_clock;
        EventQueue* eventQueue = GetSystem<EventQueue>();
        Clock::time_point last = Clock::now();

        while (!MainWindow->ShouldClose())
        {
            const Clock::time_point now = Clock::now();
            const float frameTime = std::chrono::duration<float>(now - last).count();
            last = now;
            const float dt = (std::min)(frameTime, MaxDeltaTime);
#ifdef _DEBUG
            if (frameTime > MaxDeltaTime)
                xg::Log(xg::MessageType::Info, "Frame took %.2f s; delta capped to %.2f s", frameTime, MaxDeltaTime);
#endif

            if (eventQueue)
                eventQueue->Clear();
            MainWindow->PollEvents();

            _scriptTree->Update(dt);

            if (ActorRegistry* actors = GetSystem<ActorRegistry>())
                actors->UpdateActors(dt);

            if (Renderer)
                Renderer->Render();
        }
    }

    void Engine::Shutdown()
    {
        auto* storage = static_cast<ModuleStorage*>(_moduleStorage);

        std::vector<ScriptModule*> modules;
        modules.reserve(storage->Modules.size());
        for (auto& pair : storage->Modules)
        {
            if (pair.second)
                modules.push_back(pair.second);
        }
        storage->Modules.clear();

        std::sort(modules.begin(), modules.end(), [](ScriptModule* a, ScriptModule* b)
            {
                return a->GetModuleId() > b->GetModuleId();
            });

        for (ScriptModule* module : modules)
        {
            if (ScriptNode* node = _scriptTree->FindNode(module))
                node->ShutdownAllSystems();

            ScriptHost* host = module->GetHost();
            module->Shutdown();
            delete module;
            XG_RELEASE_ONE(host);
        }

        _scriptTree->Shutdown();

        // Destroy hosts
        auto* hosts = static_cast<HostStorage*>(_hostStorage);
        for (auto& entry : hosts->Hosts)
        {
            XG_SAFE_RELEASE(entry.Host);
        }
        hosts->Hosts.clear();

        // Renderer cleanup
        Renderer.reset();        

        xg::RendererLib = nullptr;
    }

    void Engine::AddLogCallback(LogCallback cb)
    {
        _logger->AddCallback(cb);
    }

    void Engine::RemoveLogCallback(LogCallback cb)
    {
        _logger->RemoveCallback(cb);
    }

    Logger* Engine::GetLogger()
    {
        return _logger;
    }

    CodecRegistry* Engine::GetCodecRegistry(ScriptBackendType backendType) const
    {
        switch (backendType)
        {
        case xg::ScriptBackendType::CoreCLR:
            return _coreclrCodecRegistry;
        case xg::ScriptBackendType::Native:
			return _nativeCodecRegistry;
        default:
            break;
        }
        return nullptr;
    }

    XG_DECLARE_UNIQUE_TYPE(DynamicObjectImpl)
    DynamicObjectUnique Engine::CreateDynamic(const TypeSchema* schema)
    {
        return DynamicObjectUnique(
            xgUniqueHandle{ MakeDynamicObjectImplUniqueHandle(new DynamicObjectImpl(schema)) }
        );
    }

}
