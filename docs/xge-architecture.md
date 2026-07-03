# 📊 Architecture Update Overview

**Completed**
- ScriptTree - Hierarchical Script Modules (HSM)  
- Messenger  
- Route Traversal  
- Codec Registry  
- Shared World Data

**In Progress**
- Dispatch Groups (thread‑affinity domains)

**Why It Matters**
- Establishes core runtime model  
- Enables multi‑threaded, multi‑instance architecture  
- Provides deterministic communication + data flow

---

# 🎮 Hierarchical Script Modules (HSM)

**What It Is**  
A hierarchical tree of ScriptModules defining ownership, update order, communication scope, and runtime domains.

**Key Features**
- Deterministic parent → child updates  
- Scoped communication boundaries  
- Optional thread affinity  
- Hot‑reload boundaries  
- Renderer‑independent

**Benefits**
- Predictable execution  
- Natural system grouping  
- Works identically in client, editor, server

---

# 🏭 HSM for Game Instancing (Headless Server Mode)

**Why It Works**
- Multiple game worlds per process  
- Isolated gameplay loops  
- Deterministic authoritative logic  
- No renderer dependency

**Example**
GameServerModule → N GameInstanceModules (each with its own world + systems)

---

# 🧩 What ScriptModules Are Used For

**Common Uses**
- Gameplay systems (AI, combat, inventory)  
- Entire game worlds  
- Editor tools (Inspector, Console)  
- UI/HUD logic  
- Networking/replication  
- Scripting runtimes (CoreCLR, Lua, Python)  
- Platform integration (Steam, Discord)

**Why It Matters**
- Unified lifecycle  
- Works across all ScriptHosts  
- Fits naturally into HSM + Messenger

---

# 🌐 Shared World Data via HSM

**Shared Data Types**
- WorldStateModule (authoritative state)  
- SharedCacheModule (navmesh, static map data)  
- SharedConfigModule (balance/tuning data)  
- SharedServicesModule (time, random, asset registry)

**How It Works**
- Parent owns shared data  
- Children read/request via Messenger  
- Reflection ensures safe structured exchange  
- Immutable data shared across server instances

---

# 🧩 ScriptHosts

**Built‑In**
- CoreCLR Host  
- Native Host

**Supported by Design**
- Lua  
- Squirrel  
- Python  
- JavaScript  
- WASM

**What a ScriptHost Is**  
A ScriptHost is the runtime bridge between the engine and a scripting backend. It loads modules (DLLs, assemblies, scripts), creates ScriptModule instances, owns the backend runtime (CLR, VM, interpreter, WASM sandbox), binds engine services (Messenger, reflection, world data), and manages module/runtime lifetime. ScriptHosts make the engine backend‑agnostic and allow multiple runtimes to coexist with identical behavior.

**Why It Works**
- Unified ScriptModule interface  
- Dynamic loading  
- Shared messaging + reflection

---

# 🔄 Messenger

**Routing Modes**
- Parent → child  
- Child → parent  
- Sibling ↔ sibling  
- Cross‑thread queued delivery

**Benefits**
- Decoupled modules  
- Editor ↔ runtime communication  
- Works across all ScriptHosts

---

# 🏷️ Encoder / Decoder (Reflection Serialization)

**Reflection Metadata**
RawFieldInfo: name, key, offset, type

**Encoding**
1. Iterate fields  
2. Resolve offsets  
3. Write primitives

**Decoding**
1. Read JSON  
2. Resolve offsets  
3. Assign into struct memory

**Benefits**
- Zero boilerplate  
- C++ + C# compatible  
- Deterministic + engine‑agnostic

---

# 🔗 System Integration

**Flow**
1. HSM defines module structure  
2. Messenger routes communication  
3. Codec Registry encodes/decodes data  
4. ScriptHosts execute modules  
5. Dispatch Groups (soon) define thread domains

**Example**
Gameplay → PlayerState → Encode → Route → Decode → Editor/Server

---

# 🚀 Architectural Benefits

**Unified Model**
- One hierarchy  
- One messaging layer  
- One serialization pipeline  
- Multiple runtimes

**Symmetry**
- Same message types  
- Same routing rules  
- Same reflection metadata  
- Same ScriptModule interface

**Scalability**
- Prototypes → AAA  
- Multi‑instance servers  
- Multi‑runtime scripting  
- Ready for hot reload + multi‑threading
