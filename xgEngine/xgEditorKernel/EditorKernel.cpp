// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"
#include "EditorKernel.h"
#include "xgEngine.h"
#include "xgScriptHost.h"
#include "xgScriptModule.h"
#include "xgMemberCallback.h"
#include "xgEventDispatcher.h"
#include "xgListenerBinding.h"
#include "xgMessenger.h"
#include "xgRoute.h"
#include "xgCodecRegistry.h"
#include "xgSceneGraph.h"
#include "xgSceneGraphFactory.h"
#include "xgActorRegistry.h"

#include "xgEngineDef.h"
#include "xgEngineDef.generated.h"

#include "xgHierarchyNode.h"
#include "xgComponentSceneGraph.h"
#include "xgVec3.h"
#include "xgImage.h"
#include "xgImUI.h"
#include "NativeTestWindow.h"
#include "SceneViewWindow.h"
#include "EditorSceneViewExports.h"
#include "xgPlatformServices.h"

namespace xg
{
    //
    // ------------------------------------------------------------
    // EditorKernelModule
    // ------------------------------------------------------------
    //

    EditorKernelModule::EditorKernelModule(const char* id, int moduleID, ScriptHost* host, const char* group)
        : ScriptModule(id, moduleID, host, group)
    {}

    EditorKernelModule::~EditorKernelModule()
    {
        Shutdown();
    }

    static const xg::FieldSchema TestFields[] =
    {
        { "Health", "Int32", xg::ValueType::Int32, 0, sizeof(int32_t), alignof(int32_t), {nullptr} },
        { "Name",   "String", xg::ValueType::String, 0, sizeof(const char*), alignof(const char*), {nullptr} }
    };

    static const xg::TypeSchema TestSchema =
    {
        "TestType",
        1,
        false,              // IMPORTANT: dynamic layout
        TestFields,
        2,
        0,
        0
    };

    bool EditorKernelModule::Init(ScriptEngine* engine)
    {
        if (!engine)
            return false;

        _engine = engine;
        if (const EngineConfig* config = engine->GetEngineConfig())
        {
            _prototypeUI = config->EditorUI;
            _multiViewport = config->EditorMultiViewport;
        }

        //
        // Create event listener (modernized)
        //
        _eventListener = std::make_unique<ListenerBinding<EditorKernelModule>>(
            this,
            &EditorKernelModule::OnEvent
        );

        engine->GetSubsystem<EventDispatcher>()->AddListener(_eventListener.get());

        //
        // Editor UI setup
        //
        InitUI();

		/*auto sceneGraphFactory = engine->GetSubsystem<SceneGraphFactory>(this);
		auto actorRegistry = engine->GetSubsystem<ActorRegistry>(this);
        auto sceneGraph = sceneGraphFactory->Create("tree", *actorRegistry);
		engine->RegisterSubsystem<SceneGraph>(nullptr, sceneGraph);

        auto sceneRoot = xg::CreateNode<HierarchyNode>(*sceneGraph);
        sceneRoot->Name = "mr root";*/
        //
        // Load managed editor module
        //
        const char* group = GetGroup();
        _gameKernel = _engine->AddScriptModule("game.kernel", "xgGameKernel.dll", this, group);
        _editorModule = _engine->AddScriptModule("editor", "xgEditor.CoreCLR.dll", _gameKernel, group);
        _engine->AddScriptModule("game.demo", "GameDemoModule.dll", _gameKernel, group);

       /* auto componentSG = sceneGraphFactory->Create("compset", *actorRegistry);
        auto componentSet = static_cast<ComponentSceneGraph*>(componentSG);
        engine->RegisterSubsystemWithAlias(nullptr, componentSet, "PlayerComponent");
        
        componentSet->InitializeComponentSet("PlayerComponent", 12);
        ComponentSetNode* co = componentSet->CreateComponent<ComponentSetNode>(55);*/
        //xg::TypeRegistry::Register(&xg::TypeInfo<PlayerState>::Instance);

        CodecRegistry* registry = _host->GetCodecRegistry();

        // Register generic JSON codec
        //registry

        PlayerState state{ 100, 10.0f, 20.0f };

        // Broadcast to all modules
        //const TypeSchema* schema =
            //engine->GetTypeRegistry()->Get("*");

        /*ScriptMessage msg{};
        registry->Encode("PlayerState", PayloadMode::JSON, &state, schema, msg);

        printf("Encoded JSON: %s\n", msg.Payload);

        PlayerState dst{};
        registry->Decode("PlayerState", PayloadMode::JSON, msg, schema, &dst);*/

        auto obj = _engine->CreateDynamic(&TestSchema);

        obj->FindField("Health")->Value.Int32Value = 150;
        obj->FindField("Name")->Value.StringValue = "John";
        
        xg::ScriptMessage msg;
        msg.Payload = nullptr;
        msg.PayloadSize = 0;
        msg.SourceID = GetModuleId();
        bool ok = registry->Encode("TestType", obj.get(), &TestSchema, msg);

        if (!ok)
            printf("Encode failed\n");
        else
        {
            printf("Encoded JSON (%d bytes):\n", msg.PayloadSize);
            fwrite(msg.Payload, 1, msg.PayloadSize, stdout);
            printf("\n");
        }

		//TODO release dynamic object

        _engine->GetSubsystem<Messenger>(this)->SendToAll(msg);
        return true;
    }

    void EditorKernelModule::Shutdown()
    {
        if (!_isValid)
            return;

        //
        // Unregister event listener
        //
        if (_engine)
        {
            if (auto dispatcher = _engine->GetSubsystem<EventDispatcher>())
                dispatcher->RemoveListener(_eventListener.get());
        }

        _eventListener.reset();

        //
        // Shutdown editor UI
        //
        ShutdownUI();

        //
        // Remove managed editor module
        //
        if (_engine && _editorModule)
        {
            _engine->RemoveScriptModule("editor");
            _editorModule = nullptr;
        }
        SetSceneViewRenderer(nullptr);

        _engine = nullptr;
        _isValid = false;
    }

    //
    // ------------------------------------------------------------
    // Update
    // ------------------------------------------------------------
    //
    void EditorKernelModule::Update(float dt)
    {
        if (_uiReady)
            XG_ImUIHostFrame(dt);
    }

    SceneGraph* EditorKernelModule::GetSceneGraph()
    {
        return _engine->GetSubsystem<SceneGraph>(this);
    }

    //
    // ------------------------------------------------------------
    // Editor UI Init / Shutdown
    // ------------------------------------------------------------
    //
    void EditorKernelModule::InitUI()
    {
        auto* engine = static_cast<Engine*>(_engine);
        Window* window = engine->MainWindow.get();
        if (!window)
            return;

        if (engine->Renderer.valid())
        {
            _uiTarget = engine->Renderer.get();
        }
        else
        {
            _ownedUITarget = xg::NewUIRenderTarget(*window);
            _uiTarget = _ownedUITarget.get();
        }
        if (!_uiTarget)
        {
            xg::Log(xg::MessageType::Error, "EditorKernel: no UI render target available, editor UI disabled");
            return;
        }

        imui::HostDesc desc = {};
        desc.Size = sizeof(desc);
        desc.MainWindow = window;
        desc.RenderTarget = _uiTarget;
        desc.Flags = (_prototypeUI ? imui::HostFlags::PrototypeShell : imui::HostFlags::None)
            | (_multiViewport ? imui::HostFlags::MultiViewport : imui::HostFlags::None);
        desc.Platform = GetPlatformServices();
        desc.FontDirectory = "../editor/fonts";
        if (XG_ImUIGetApiVersion() < imui::ApiVersion || !XG_ImUIHostInit(&desc))
        {
            xg::Log(xg::MessageType::Error, "EditorKernel: xgImUIKit initialization failed, editor UI disabled");
            _uiTarget = nullptr;
            _ownedUITarget.reset();
            return;
        }
        _uiReady = true;

        ImageRGBA icon = {};
        if (LoadImageRGBA("../editor/xge-icon.bmp", &icon))
        {
            XG_ImUIHostSetAppIcon(icon.Pixels, icon.Width, icon.Height);
            FreeImageRGBA(&icon);
        }

        _nativeTestWindow = std::make_unique<NativeTestWindow>();
        _nativeFloatingWindow = std::make_unique<NativeFloatingWindow>();

        SetSceneViewRenderer(engine->Renderer.valid() ? engine->Renderer.get() : nullptr);
        const float clearA[3] = { 0.16f, 0.18f, 0.22f };
        const float clearB[3] = { 0.22f, 0.17f, 0.15f };
        const bool outside = TestFloatOutside();
        _sceneViewWindows[0] = std::make_unique<SceneViewWindow>("Scene View 1", "Tests/Scene View 1",
            outside ? Rect::FromXYWH(-200.0f, 400.0f, 400.0f, 280.0f) : Rect::FromXYWH(20.0f, 330.0f, 400.0f, 300.0f), 0.6f, clearA, outside);
        _sceneViewWindows[1] = std::make_unique<SceneViewWindow>("Scene View 2", "Tests/Scene View 2", Rect::FromXYWH(440.0f, 330.0f, 400.0f, 300.0f), 2.4f, clearB);
    }

    void EditorKernelModule::ShutdownUI()
    {
        if (!_uiReady)
            return;
        _uiReady = false;

        for (auto& window : _sceneViewWindows)
            window.reset();
        _nativeFloatingWindow.reset();
        _nativeTestWindow.reset();
        XG_ImUIHostShutdown();
        _uiTarget = nullptr;
        _ownedUITarget.reset();
    }

    //
    // ------------------------------------------------------------
    // Event Handling
    // ------------------------------------------------------------
    //
    void EditorKernelModule::OnEvent(const xgEvent& e)
    {
        if (_uiReady)
            XG_ImUIHostEvent(&e);
    }

    void EditorKernelModule::OnMessage(const ScriptMessage& msg)
    {    
		xg::CodecRegistry* codecs = _host->GetCodecRegistry();
        auto obj2 = _engine->CreateDynamic(&TestSchema);

        bool ok2 = codecs->Decode("TestType", msg, &TestSchema, &obj2);

        if (!ok2)
            printf("Decode failed\n");
        else
        {
            printf("Decoded Health = %d\n", obj2->FindField("Health")->Value.Int32Value);
            printf("Decoded Name   = %s\n", obj2->FindField("Name")->Value.StringValue);
        }
    }
}

//
// ------------------------------------------------------------
// ScriptHostNative factory
// ------------------------------------------------------------
//
extern "C"
XG_MODULE_EXPORT xg::ScriptModule* XG_CreateScriptModule(const char* id, int moduleID, xg::ScriptHost* host, const char* group)
{
    if (host && host->GetEngine())
        xg::SetLogger(host->GetEngine()->GetLogger());
    return new xg::EditorKernelModule(id, moduleID, host, group);
}
