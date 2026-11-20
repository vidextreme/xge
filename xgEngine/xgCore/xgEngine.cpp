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
#include "ResourceSystemImpl.h"
#include "JobSystemImpl.h"
#include "TextureResources.h"
#include "ModelResources.h"
#include "LoggerImpl.h"
#include "SceneGraphFactoryImpl.h"

#include "TreeSceneGraph.h"
#include "CompositeSceneGraph.h"

#include "CodecJSON.h"
#include "CodecBinary.h"
#include "xgEnv.h"

#include <atomic>
#include <cmath>
#include <cstdlib>
#include <set>
#include <thread>

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

        char stepLimit[16] = {};
        if (xg::GetEnv("XG_UI_TEST_SPLASH_STEP", stepLimit, sizeof(stepLimit)))
            _startupStepLimit = std::atoi(stepLimit);

        //MainWindow = NewWindow("xgEngine", 1280, 720);
        _moduleStorage = new ModuleStorage();
        _hostStorage = new HostStorage();

        _scriptTree = new ScriptTree();
        //_codec = new JSONMessageCodec();
        //_messenger = new MessengerImpl(_scriptTree);
        _rootModule = AddScriptModule("root", nullptr, nullptr, "core");
		_rootNode = _scriptTree->FindNode(_rootModule);
        //auto ha = MessengerImpl::SuperTypeID;
		RegisterSubsystem(_rootModule, new MessengerImpl(_scriptTree));
        RegisterSubsystem(_rootModule, new ActorRegistryImpl());
        auto* jobs = new JobSystemImpl();
        RegisterSubsystem(_rootModule, jobs);
        RegisterSubsystem(_rootModule, new ResourceSystemImpl(jobs));
        
        auto eventDispatcher = new EventDispatcherImpl();
        RegisterSubsystem(_rootModule, eventDispatcher);

		auto sceneGraphFactory = new SceneGraphFactoryImpl();
        RegisterSubsystem(_rootModule, sceneGraphFactory);

		auto queue = new EventQueueImpl();
        RegisterSubsystem(_rootModule, queue);

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
            MainWindow->SetEventQueue(GetSubsystem<EventQueue>());
            MainWindow->SetEventDispatcher(GetSubsystem<EventDispatcher>());
        }

        CodecRegistry* registry = GetCodecRegistry(ScriptBackendType::CoreCLR);
        PayloadMode mode = GetPayloadMode();
        if (mode == PayloadMode::JSON)
        {
            // Register generic JSON codec
            registry->RegisterEncoder("*", XG_Encode_JSON_Generic);
            registry->RegisterDecoder("*", XG_Decode_JSON_Generic);
        }
        else if (mode == PayloadMode::BINARY)
        {
            // Register generic binary codec
            registry->RegisterEncoder("*", XG_Encode_Binary_Generic);
            registry->RegisterDecoder("*", XG_Decode_Binary_Generic);
        }
        
        if (config.RendererModule && !SetRendererModule(config.RendererModule))
            return false;

        if (xg::EnvEquals("XG_JOB_TEST", "1"))
            RunJobTest();

        return true;
    }

    bool Engine::SetRendererModule(const char* path)
    {
        ResourceSystem* resources = GetSubsystem<ResourceSystem>();
        if (resources && _modelType != 0)
            resources->UnregisterType(_rootModule->GetModuleId(), _modelType);
        if (resources && _textureType != 0)
            resources->UnregisterType(_rootModule->GetModuleId(), _textureType);
        _textureType = 0;
        _modelType = 0;
        Renderer.reset();

        xg::RendererDLL = path;
        xg::RendererLib = nullptr;

        Renderer = CreateRenderer(*MainWindow);
        if (!Renderer.valid())
            return false;
        if (resources)
        {
            _textureType = RegisterTextureResources(*resources, _rootModule->GetModuleId(), Renderer.get());
            _modelType = RegisterModelResources(*resources, _rootModule->GetModuleId(), Renderer.get());
        }
        return true;
    }

    namespace
    {
        struct JobTest
        {
            static constexpr int Count = 16;
            std::atomic<int> Ran{ 0 };
            std::mutex Mutex;
            std::set<std::thread::id> Threads;
            std::thread::id Main;
        };

        void RunJobTestWork(void* user)
        {
            auto* test = static_cast<JobTest*>(user);
            std::this_thread::sleep_for(std::chrono::milliseconds(4));
            {
                std::lock_guard<std::mutex> lock(test->Mutex);
                test->Threads.insert(std::this_thread::get_id());
            }
            ++test->Ran;
        }

        void RunJobTestJoin(void* user)
        {
            auto* test = static_cast<JobTest*>(user);
            xg::Log(xg::MessageType::Info, "[Jobs] continuation after %d/%d jobs, on the main thread: %s, worker threads used: %d",
                test->Ran.load(), JobTest::Count, std::this_thread::get_id() == test->Main ? "yes" : "no", static_cast<int>(test->Threads.size()));
            delete test;
        }
    }

    // XG_JOB_TEST=1: fan out worker jobs, join them in a main-thread continuation (run by the first frame's PumpMain).
    void Engine::RunJobTest()
    {
        JobSystem* jobs = GetSubsystem<JobSystem>();
        if (!jobs)
            return;
        auto* test = new JobTest();
        test->Main = std::this_thread::get_id();
        std::vector<JobHandle> work;
        for (int i = 0; i < JobTest::Count; ++i)
        {
            const JobDesc desc{ &RunJobTestWork, test, nullptr, 0, JobQueue::Worker, 0 };
            work.push_back(jobs->Schedule(&desc));
        }
        const JobDesc join{ &RunJobTestJoin, test, work.data(), static_cast<uint32_t>(work.size()), JobQueue::Main, 0 };
        const JobHandle joined = jobs->Schedule(&join);
        xg::Log(xg::MessageType::Info, "[Jobs] %u worker(s), scheduled %d jobs and a continuation (done before the frame: %s)",
            jobs->WorkerCount(), JobTest::Count, jobs->IsDone(joined) ? "yes" : "no");
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

    Subsystem* Engine::GetSubsystemImpl(TypeID id, ScriptModule* module)
    {
        if (module == nullptr)
            return _rootNode->GetSubsystem(id);

        Subsystem* system = nullptr;
        ScriptNode* node = _scriptTree->FindNode(module);
        if (node)
        {
            system = node->GetSubsystem(id);
            if(system == nullptr)
                system = node->GetParent()->GetSubsystem(id);
        }
        return system;
    }

    Subsystem* Engine::GetSubsystemImpl(TypeID id, const char* moduleID)
    {
		ScriptModule* module = GetScriptModule(moduleID);       

        return GetSubsystemImpl(id, module);
    }

    void Engine::RegisterSubsystemImpl(ScriptModule* module, TypeID id, Subsystem* system)
    {
        if (module == nullptr)
        {
            _rootNode->RegisterSubsystem(id, system);
            return;
        }

        ScriptNode* node = _scriptTree->FindNode(module);
        if (node)
        {
            node->RegisterSubsystem(id, system);
        }
    }

    bool Engine::UnregisterSubsystemImpl(ScriptModule* module, TypeID id)
    {
        if (module == nullptr)
            return _rootNode->ShutdownSubsystem(id);

        ScriptNode* node = _scriptTree->FindNode(module);
        return node ? node->ShutdownSubsystem(id) : false;
    }

    ScriptModule* Engine::GetParentScriptModule(ScriptModule* module) const
    {
        ScriptNode* node = module ? _scriptTree->FindNode(module) : nullptr;
        ScriptNode* parent = node ? node->GetParent() : nullptr;
        return parent ? parent->GetModule() : nullptr;
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

        return AttachModule(module, parent) ? module : nullptr;
    }

    uint32_t Engine::AllocateModuleId()
    {
        return ++_nextModuleId;
    }

    bool Engine::AddExternalScriptModule(ScriptModule* module, ScriptModule* parent)
    {
        if (!module)
            return false;
        if (!module->IsValid() || !module->GetId() || GetScriptModule(module->GetId()))
        {
            xg::Log(xg::MessageType::Error, "Cannot add script module %s", module->GetId() ? module->GetId() : "<null>");
            ScriptHost* host = module->GetHost();
            delete module;
            XG_RELEASE_ONE(host);
            return false;
        }
        if (ScriptHost* host = module->GetHost())
            host->AddRef();
        xg::Log(xg::MessageType::Info, "Added script module: %s (id %u)", module->GetId(), module->GetModuleId());
        return AttachModule(module, parent ? parent : _rootModule);
    }

    // Registers the module, inserts it into the tree and initializes it (in that order, so parent lookups work during
    // Init). On failure the module is deleted and its host reference released.
    bool Engine::AttachModule(ScriptModule* module, ScriptModule* parent)
    {
        auto* storage = static_cast<ModuleStorage*>(_moduleStorage);
        const char* id = module->GetId();
        ScriptHost* host = module->GetHost();
        storage->Modules[id] = module;

        ScriptNode* node = _scriptTree->AddModule(module, parent);
        if (!node)
        {
            xg::Log(xg::MessageType::Error, "ScriptTree failed to attach module: %s", id);
            storage->Modules.erase(id);
            delete module;
            XG_RELEASE_ONE(host);
            return false;
        }

        if (!module->Initialize(this))
        {
            storage->Modules.erase(id);
            module->Shutdown();
            node->ShutdownAllSubsystems();
            _scriptTree->RemoveModule(module);
            delete module;
            XG_RELEASE_ONE(host);
            return false;
        }

        _scriptTree->ForEachNodeRecursive(node, [](ScriptNode* n)
            {
                n->ForEachSubsystem([](Subsystem* system)
                    {
                        system->Initialize();
                    });
            });
        return true;
    }

    // Shuts the module down while its node and subsystems still exist, then releases what it owns and deletes it.
    // The node stays in the tree; the caller removes it.
    void Engine::DestroyModule(ScriptModule* module)
    {
        ScriptHost* host = module->GetHost();
        module->Shutdown();
        if (JobSystem* jobs = GetSubsystem<JobSystem>())
            jobs->WaitModule(module->GetModuleId());
        if (ResourceSystem* resources = GetSubsystem<ResourceSystem>())
            resources->ReleaseModule(module->GetModuleId());
        if (ScriptNode* node = _scriptTree->FindNode(module))
            node->ShutdownAllSubsystems();
        delete module;
        XG_RELEASE_ONE(host);
    }

    void Engine::RunBetweenFrames(void (*fn)(void* user), void* user)
    {
        if (fn)
            _betweenFrames.emplace_back(fn, user);
    }

    ScriptNode* Engine::FindModuleNode(const char* id)
    {
        ScriptModule* module = id ? GetScriptModule(id) : nullptr;
        return module ? _scriptTree->FindNode(module) : nullptr;
    }

    bool Engine::SetModulePaused(const char* id, bool paused)
    {
        ScriptNode* node = FindModuleNode(id);
        if (!node)
            return false;
        node->SetPaused(paused);
        return true;
    }

    bool Engine::IsModulePaused(const char* id)
    {
        ScriptNode* node = FindModuleNode(id);
        return node && node->IsPaused();
    }

    bool Engine::StepModule(const char* id)
    {
        ScriptNode* node = FindModuleNode(id);
        if (!node)
            return false;
        node->RequestStep();
        return true;
    }

    void Engine::SetActorUpdatesPaused(bool paused)
    {
        _actorsPaused = paused;
    }

    bool Engine::AreActorUpdatesPaused() const
    {
        return _actorsPaused;
    }

    void Engine::StepActorUpdates()
    {
        _actorStep = true;
    }

    float Engine::ActorFrameDelta(void* user, uint32_t moduleId, float dt)
    {
        Engine* engine = static_cast<Engine*>(user);
        if (engine->_actorsPaused)
        {
            if (!engine->_actorStep)
                return -1.0f;
            dt = ScriptNode::StepDeltaTime;
        }
        ScriptModule* module = engine->GetScriptModule(moduleId);
        ScriptNode* node = module ? engine->_scriptTree->FindNode(module) : nullptr;
        return node ? node->FrameDelta(dt) : dt;
    }

    void Engine::DrainBetweenFrames()
    {
        if (_betweenFrames.empty())
            return;
        std::vector<std::pair<void (*)(void*), void*>> pending;
        pending.swap(_betweenFrames);
        for (auto& [fn, user] : pending)
            fn(user);
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
        if (it == storage->Modules.end())
            return;
        ScriptModule* module = it->second;
        if (!module || module == _rootModule)
        {
            if (!module)
                storage->Modules.erase(it);
            return;
        }

        std::vector<ScriptModule*> removed;
        _scriptTree->CollectDescendants(module, removed);
        removed.push_back(module);
        for (ScriptModule* m : removed)
        {
            xg::Log(xg::MessageType::Info, "Removing script module: %s", m->GetId());
            for (auto entry = storage->Modules.begin(); entry != storage->Modules.end(); ++entry)
            {
                if (entry->second == m)
                {
                    storage->Modules.erase(entry);
                    break;
                }
            }
        }
        for (ScriptModule* m : removed)
            DestroyModule(m);
        _scriptTree->RemoveModule(module);
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
        static constexpr float MainJobBudget = 0.008f;

        using Clock = std::chrono::steady_clock;
        EventQueue* eventQueue = GetSubsystem<EventQueue>();
        Clock::time_point last = Clock::now();

        double splashHold = 0.0;
        char hold[32] = {};
        if (Splash && xg::GetEnv("XG_UI_TEST_SPLASH_HOLD", hold, sizeof(hold)))
            splashHold = (std::max)(std::atof(hold), 0.0);
        const Clock::time_point splashUntil = last + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(splashHold));
        ReportStartup("Ready", "", 1.0f);

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

            if (JobSystem* jobs = GetSubsystem<JobSystem>())
                jobs->PumpMain(MainJobBudget);

            _scriptTree->Update(dt);

            if (ActorRegistry* actors = GetSubsystem<ActorRegistry>())
                actors->UpdateActorsPerModule(dt, &Engine::ActorFrameDelta, this);
            _scriptTree->ClearSteps();
            _actorStep = false;

            DrainBetweenFrames();

            if (Renderer)
                Renderer->Render();

            if (ResourceSystem* resources = GetSubsystem<ResourceSystem>())
                resources->Collect();

            if (Splash)
            {
                if (Clock::now() >= splashUntil)
                    CloseSplash();
                else
                    Splash->Pump();
            }
        }
        CloseSplash();
    }

    void Engine::CloseSplash()
    {
        if (!Splash)
            return;
        if (MainWindow)
            MainWindow->Show();
        Splash.reset();
    }

    void Engine::ReportStartup(const char* step, const char* detail, float progress)
    {
        if (!Splash)
            return;
        if (_startupStepLimit >= 0 && _startupSteps > _startupStepLimit)
            return;
        if (step && _startupStepLimit >= 0 && ++_startupSteps > _startupStepLimit)
            return;
        if (!std::isnan(progress))
            _startupProgress = progress;
        Splash->SetStep(step, _startupProgress);
        if (detail)
            Splash->SetDetail(detail);
        Splash->Pump();
    }

    void Engine::PumpStartup()
    {
        if (Splash)
            Splash->Pump();
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
                node->ShutdownAllSubsystems();

            ScriptHost* host = module->GetHost();
            module->Shutdown();
            if (JobSystem* jobs = GetSubsystem<JobSystem>())
                jobs->WaitModule(module->GetModuleId());
            if (ResourceSystem* resources = GetSubsystem<ResourceSystem>())
                resources->ReleaseModule(module->GetModuleId());
            delete module;
            XG_RELEASE_ONE(host);
        }

        if (JobSystem* jobs = GetSubsystem<JobSystem>())
            jobs->WaitIdle();
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
