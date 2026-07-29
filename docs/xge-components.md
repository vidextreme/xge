# **XGE ECS Architecture — Formal Specification**

---

# **1. Actor Layer (C++)**

### **Responsibilities**
- Owns `ActorID` lifecycle  
- Maintains hierarchy  
- Stores transforms  
- Stores metadata  
- Hosts component attachments  

### **Interface**
```cpp
class SceneGraph {
public:
    virtual SceneNode* GetNode(ActorID id) const = 0;
    virtual void SetParent(SceneNode* child, SceneNode* parent) = 0;
    virtual void Traverse(SceneNode* root, VisitorFn fn) = 0;
};
```

---

# **2. ScriptModule Hierarchy**

```
EngineRootModule
└── WorldScriptModule
    ├── EditorKernelModule
    │   ├── EditorViewportModule
    │   ├── EditorSelectionModule
    │   ├── EditorGizmoModule
    │   ├── EditorInspectorModule
    │   └── EditorToolsModule
    │
    └── RuntimeKernelModule
        ├── GameplayModule
        ├── PhysicsModule
        ├── AIModule
        └── UIModule
```

SceneGraph attaches to `WorldScriptModule`.

---

# **3. Component Model**

## **3.1 Base Class**
```csharp
public abstract class Component
{
}
```

## **3.2 Component<TData>**
```csharp
public abstract class Component<TData> : Component
    where TData : struct
{
    public TData Data;
}
```

### **Properties**
- Used by Archetype ECS  
- Used by Dynamic ECS  
- `TData` is blittable  
- Stored in native ECS memory  
- Component class resides in sidecar  

---

# **4. ECS Storage Model**

## **4.1 Archetype Component Graph**

### **Definition**
A graph whose stride is the composition of multiple `Component<TData>` types defined by an archetype.

### **Stride**
```
Stride = sum(sizeof(TData_i)) for all components in the archetype
```

### **Source**
Prefab-defined archetypes.

### **Metadata**
- Component list  
- Stable ordering  
- Offsets  
- Stride size  
- Alignment  
- Marshaling metadata  

### **Memory Layout**
```
[Stride][Stride][Stride]...
```

### **Structures**
```cpp
struct ArchetypeInfo {
    size_t Stride;
    size_t Alignment;
    std::vector<ComponentType> ComponentTypes;
    std::vector<size_t> ComponentOffsets;
};

struct Chunk {
    void* Data;
    int Capacity;
    int Count;
    FreeList FreeSlots;
};
```

---

## **4.2 Dynamic Component Graph**

### **Definition**
A graph where each component type has its own storage, and the stride is the size of that component’s `TData`.

### **Stride**
```
Stride = sizeof(TData)
```

### **Source**
Engine-registered component types.

### **Memory Layout**
```
[TData][TData][TData]...
```

### **Structures**
```cpp
struct DynamicComponentGraph {
    size_t ComponentSize;
    size_t Alignment;
    std::vector<uint8_t> DenseStorage;
    std::vector<ActorID> Owner;
    FreeList FreeSlots;
};
```

---

# **5. Sidecar (Managed C#)**

### **Contents**
- Component<TData> class instances  
- Managed fields  
- Script logic  
- RPC methods  
- Message handlers  

### **Properties**
- Indexed by ActorID externally  
- Synchronized with ECS TData  

---

# **6. Marshaling**

### **Static and Dynamic Components**
```
Unsafe.Write(ptr, component.Data);
component.Data = Unsafe.Read<TData>(ptr);
```

---

# **7. ActorID**

### **Structure**
```cpp
struct ActorID {
    uint Index;
    uint Generation;
};
```

### **Usage**
Static ECS:
```csharp
public struct EnemyAIData {
    public ActorID Target;
}
```

Dynamic ECS:
```cpp
Owner[index] = actorId;
```

Sidecar:
```csharp
var health = World.GetComponent<HealthComponent>(targetActorId);
```

---

# **8. Component Access**

### **Same Actor**
```
var health = World.GetComponent<HealthComponent>(actorId);
```

### **Cross-Actor**
```
var health = World.GetComponent<HealthComponent>(targetActorId);
```

### **Lookup**
- ActorID → ECS index  
- ECS index → sidecar instance  

---

# **9. Systems**

### **C# Systems**
- Operate on archetype chunks  
- Operate on dynamic component graphs  
- Synchronize with sidecar  

### **C++ Systems**
- Physics  
- Rendering  
- Animation  
- Audio  

---

# **10. Reflection Integration**

### **Component<TData>**
- Managed fields  
- Editor UI  
- Serialization  
- Messaging  
- RPC metadata  
- Script binding  

---

# **11. Spawning Pipeline**

1. Generate ActorID  
2. Register ActorID → ScriptNode*  
3. SceneGraph creates node  
4. Archetype ECS allocates stride block  
5. Dynamic ECS registers per-component graphs  
6. Sidecar initializes Component<TData> instances  

---

# **12. NativeAOT Integration**

- Archetype memory exposed as `void*`  
- Fixed offsets  
- Direct read/write  
- Zero marshalling  
- ScriptModules access ECS + SceneGraph via C ABI  

---

# **13. Architecture Diagram**

```
SceneGraph (C++)
 ├─ ActorID
 ├─ Hierarchy
 ├─ Transform
 └─ Metadata

Archetype Component Graph
 ├─ Prefab-defined
 ├─ Stride = composition of Component<TData>
 ├─ Chunk-based
 └─ Offline metadata

Dynamic Component Graph
 ├─ One graph per Component<TData> type
 ├─ Stride = sizeof(TData)
 ├─ Dense arrays
 └─ ActorID association

Sidecar (C#)
 ├─ Component<TData> instances
 ├─ Managed fields
 ├─ Reflection metadata
 └─ Script logic

Systems
 ├─ C# systems
 └─ C++ systems
```

---