# **XGE Scene Graph Architecture Summary**

## **1. ScriptModule Hierarchy**
```
EngineRootModule
└── WorldScriptModule              ← SceneGraph attached here
    ├── EditorKernelModule         ← parent of all editor modules
    │   ├── EditorViewportModule
    │   ├── EditorSelectionModule
    │   ├── EditorGizmoModule
    │   ├── EditorInspectorModule
    │   └── EditorToolsModule
    │
    └── RuntimeKernelModule        ← parent of all runtime/gameplay modules
        ├── GameplayModule
        ├── PhysicsModule
        ├── AIModule
        └── UIModule
```

**Rules:**
- **WorldScriptModule is the master root.**
- **EditorKernelModule owns all editor modules.**
- **RuntimeKernelModule owns all runtime modules.**
- **SceneGraph attaches ONLY to WorldScriptModule.**
- Editor stays alive; runtime modules hot‑reload safely.

---

## **2. SceneGraph (Abstract)**
SceneGraph is **world‑data**, not logic.

**SceneGraph provides:**
- Parent/child hierarchy  
- Local + world transforms  
- Transform propagation  
- Lifetime scoping  

**SceneGraph does NOT store:**
- gameplay data  
- components  
- physics  
- rendering  
- AI  

SceneGraph is a **pure abstract interface**, allowing multiple implementations:
- tree  
- DAG  
- ECS transform graph  
- flat arrays  
- nested worlds  
- preview worlds  

---

## **3. Actor Identity**
Actors are **identity + hierarchy + transform**, nothing else.

**Actor contains:**
- Global **ActorID**  
- SceneGraph node  
- Pointer to archetype instance  
- Pointer to dynamic components  

**Actor does NOT contain gameplay data.**

---

## **4. Global ActorID Registry**
Engine maintains:
- **Global ActorID generator**  
- **Global ActorID → ScriptNode*** lookup table  

Used for:
- editor selection  
- debugging  
- messaging  
- serialization  
- hot‑reload recovery  
- multi‑world support  

Lookup is **non‑frequent**.

---

## **5. Components**
Components are **pure data**, no behavior.

Two categories:

### **A. Archetype Components (fast path)**
- predefined  
- fixed layout  
- compact contiguous block  
- cache‑friendly  
- ECS‑friendly  
- used for common components  

### **B. Dynamic Components (slow path)**
- optional  
- stored separately  
- sparse  
- slower access  
- used for rare features  

---

## **6. Archetypes**
Archetype defines:
- fixed component list  
- fixed memory layout  
- contiguous block of all components  
- offsets for each component  
- fast creation/destruction  
- ECS‑friendly iteration  

ArchetypeInstance:
```
[Transform][PhysicsBody][Render][Health]
```

Dynamic components stored separately.

---

## **7. ECS Integration**
ECS handles:
- component storage  
- SoA arrays  
- fast iteration  
- multi‑threading  
- SIMD  
- job scheduling  

**ScriptModules act as systems:**
- PhysicsModule → PhysicsBodyComponent  
- AIModule → AIStateComponent  
- RenderModule → RenderComponent  
- GameplayModule → gameplay components  

SceneGraph = hierarchy + transforms  
ECS = data + iteration  
ScriptModules = behavior

---

## **8. Spawning Pipeline**
Spawning a new object touches 4 layers:

1. **Engine**  
   - Generate ActorID  
   - Register ActorID → ScriptNode*

2. **SceneGraph**  
   - Create node  
   - Set parent  
   - Initialize transform  

3. **Archetype System**  
   - Allocate compact block  
   - Initialize components  
   - Register components in ECS  

4. **Dynamic Components (optional)**  
   - Add optional components  
   - Store in sparse pools  

Systems automatically pick up new objects.

---

## **9. NativeAOT Integration**
NativeAOT ScriptModules work **perfectly** with this architecture.

NativeAOT can:
- receive archetype memory blocks as `void*`  
- cast to component structs  
- use fixed offsets  
- read/write directly  
- operate with zero marshalling  
- run systems exactly like C++  

NativeAOT modules:
- implement ScriptModule interface  
- use static scheduling  
- access ECS + SceneGraph via C ABI  
- coexist with CoreCLR editor modules  

---

# **Final One‑Liner**
**XGE is a hybrid Actor–Archetype–ECS engine where Actors provide identity + hierarchy, SceneGraph provides transforms, Archetypes provide compact component storage, dynamic components provide flexibility, ECS provides fast iteration, and ScriptModules provide logic — all under a unified ScriptModule tree with WorldScriptModule as the master root.**
