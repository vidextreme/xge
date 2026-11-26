// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Reflection;
using System.Text.Json;
using System.Text.Json.Nodes;
using xg;
using xg.editor;

namespace xgEditor.CoreCLR
{
    // The editor's scenes (.xcs). Scenes is a list and an actor belongs to exactly one scene, so several can be loaded
    // side by side (additive loading is declared, not wired to UI yet); New and Open replace every loaded scene. Only
    // members are saved: actors created into a scene in the editor (Hierarchy create, Create from object) or loaded from
    // its file. Script, test and Composition Actors are never members. Members are created by the editor module; their
    // components are added through the component factories, so the module that defines a type adopts the actor.
    public static class SceneManager
    {
        public const string StateFile = "EditorState.json";
        private const long DirtyCheckMilliseconds = 1000;
        private const int OpenDelayFrames = 10;
        private const int SettleLimitFrames = 600;

        private static readonly List<EditorScene> _scenes = new();
        private static readonly Dictionary<(ulong Driver, string Node), SceneCompositionAsset> _pendingOverrides = new();
        private static readonly List<object> _components = new();
        private static readonly List<ActorID> _children = new();
        private static readonly List<ulong> _hidden = new();
        private static readonly List<ulong> _pushedHidden = new();
        private static string? _openAfter;
        private static int _frames;
        private static bool _hooked;
        private static long _prunedAt;

        public static IReadOnlyList<EditorScene> Scenes => _scenes;
        public static EditorScene? Active { get; private set; }
        // Raised after a scene was opened, created or saved (path may be null for Untitled).
        public static event Action<EditorScene>? Changed;

        // Called once after the project opened: reopens the last scene (after the scripts load), else an Untitled one.
        public static void Initialize()
        {
            if (!_hooked)
            {
                _hooked = true;
                CompositionComponents.Spawned += OnCompositionSpawned;
                ActorEditorState.Containers = ContainerState;
                ActorEditorState.Changed += OnEditorStateChanged;
            }
            New();
            if (ProjectService.Assets is not { } assets)
                return;
            string? last = ReadState()?["lastScene"]?.GetValue<string>();
            if (!string.IsNullOrEmpty(last) && File.Exists(assets.FullPath(last)))
                _openAfter = last;
        }

        // Opens a scene a few frames later (after the project's scripts load), replacing the loaded scenes.
        public static void OpenLater(string path) => _openAfter = path;

        public static void Update()
        {
            _frames++;
            if (Environment.GetEnvironmentVariable("XG_UI_TEST_HIERARCHY_STATE") is { Length: > 0 } stateHook)
            {
                if (_frames == 120)
                    HierarchyPanel.StateHook(stateHook);
                else if (_frames == 140)
                    Debug.Log($"[Debug] Editor hidden: {HiddenCount} actor(s)");
            }
            if (_openAfter is not null && _frames >= OpenDelayFrames)
            {
                string path = _openAfter;
                _openAfter = null;
                Open(path);
            }
            long now = Environment.TickCount64;
            if (now - _prunedAt >= DirtyCheckMilliseconds)
            {
                _prunedAt = now;
                ActorEditorState.Prune();
            }
            PushHidden();
            foreach (EditorScene scene in _scenes)
            {
                if (scene.SettleFrames > 0)
                {
                    scene.SettleFrames++;
                    if (!_pendingOverrides.Keys.Any(k => scene.Contains(new ActorID(k.Driver))) || scene.SettleFrames > SettleLimitFrames)
                    {
                        scene.SettleFrames = 0;
                        scene.Snapshot = Capture(scene).ContentJSON();
                        scene.IsDirty = false;
                    }
                    continue;
                }
                if (now - scene.CheckedAt < DirtyCheckMilliseconds || PlayMode.IsActive)
                    continue;
                scene.CheckedAt = now;
                scene.IsDirty = Capture(scene).ContentJSON() != scene.Snapshot;
            }
        }

        public static EditorScene? SceneOf(ActorID actor) => _scenes.FirstOrDefault(s => s.Contains(actor));

        public static bool IsMember(ActorID actor) => SceneOf(actor) is not null;

        public static bool AnyDirty => _scenes.Any(s => s.IsDirty);

        // Forces a dirty check now (after an edit made through the editor).
        public static void Touch(EditorScene? scene)
        {
            if (scene is null)
                return;
            scene.CheckedAt = 0;
        }

        private static bool Blocked(string action)
        {
            if (!PlayMode.IsActive)
                return false;
            Debug.LogWarning($"[Scenes] Stop play mode to {action}");
            return true;
        }

        // ------------------------------------------------------------
        // Open, new, save
        // ------------------------------------------------------------
        public static void New()
        {
            if (Blocked("create a scene"))
                return;
            UnloadAll();
            EditorScene scene = new(null, AssetMeta.NewGuid());
            _scenes.Add(scene);
            Active = scene;
            scene.Snapshot = Capture(scene).ContentJSON();
            Changed?.Invoke(scene);
        }

        public static bool Open(string path) => Load(path, false);

        // Loads a scene next to the open ones (declared for additive scenes; nothing in the UI calls it yet).
        public static bool LoadAdditive(string path) => Load(path, true);

        private static bool Load(string path, bool additive)
        {
            if (Blocked("open a scene") || ProjectService.Assets is not { } assets)
                return false;
            SceneAsset asset;
            try
            {
                asset = SceneAsset.Read(assets.FullPath(path));
            }
            catch (Exception e) when (e is IOException or JsonException or InvalidDataException or UnauthorizedAccessException)
            {
                Debug.LogError($"[Scenes] Could not read {path}: {e.Message}");
                return false;
            }
            List<string> errors = new();
            List<SceneActorAsset> ordered = asset.Ordered(errors);
            if (errors.Count > 0)
            {
                Debug.LogError($"[Scenes] {path} can't be loaded: {string.Join("; ", errors)}");
                return false;
            }
            string guid = asset.Meta?["guid"]?.GetValue<string>() ?? assets.GuidOf(path) ?? AssetMeta.NewGuid();
            if (_scenes.Any(s => s.Guid == guid))
            {
                Debug.LogWarning($"[Scenes] {path} is already loaded");
                return false;
            }
            if (!additive)
                UnloadAll();
            EditorScene scene = new(path, guid) { Meta = asset.Meta, Extra = asset.Extra.DeepClone().AsObject(), Hidden = asset.Hidden, Locked = asset.Locked };
            foreach (SceneFolderAsset saved in asset.Folders)
            {
                if (!AssetMeta.IsValidGuid(saved.Id) || scene.FindFolder(saved.Id) is not null)
                {
                    Debug.LogWarning($"[Scenes] {path}: group \"{saved.Name}\" has no valid id; dropped");
                    continue;
                }
                scene.Folders.Add(new SceneFolder { Id = saved.Id, Name = saved.Name, Parent = saved.Parent, Hidden = saved.Hidden, Locked = saved.Locked });
            }
            foreach (SceneFolder folder in scene.Folders)
                if (folder.Parent.Length > 0 && (scene.FindFolder(folder.Parent) is null || scene.IsInFolder(folder.Parent, folder.Id)))
                {
                    Debug.LogWarning($"[Scenes] {path}: group \"{folder.Name}\" names a missing or looping parent; moved to the scene root");
                    folder.Parent = string.Empty;
                }
            _scenes.Add(scene);
            if (!additive || Active is null)
                Active = scene;

            Dictionary<string, ActorID> created = new(StringComparer.OrdinalIgnoreCase);
            int components = 0;
            foreach (SceneActorAsset saved in ordered)
            {
                Actor actor = Actor.Create(saved.Name.Length > 0 ? saved.Name : "Actor");
                created[saved.Id] = actor.ID;
                scene.Members[actor.ID.Value] = saved.Id;
                ActorEditorState.Restore(actor.ID, saved.Hidden, saved.Locked);
                if (saved.Extra.Count > 0)
                    scene.ActorExtra[actor.ID.Value] = saved.Extra.DeepClone().AsObject();
                if (saved.Parent.Length == 0 && saved.Folder.Length > 0)
                {
                    if (scene.FindFolder(saved.Folder) is not null)
                        scene.FolderOf[actor.ID.Value] = saved.Folder;
                    else
                        Debug.LogWarning($"[Scenes] {path}: {saved.Name} names a missing group; kept at the scene root");
                }
                if (saved.Parent.Length > 0 && created.TryGetValue(saved.Parent, out ActorID parent))
                    ActorTree.SetParent(actor.ID, parent, false);
                if (saved.HasTransform)
                {
                    Transform transform = actor.AddTransform();
                    TransformData local = TransformData.Identity;
                    local.Position = new xg.math.Vector3(saved.Position[0], saved.Position[1], saved.Position[2]);
                    local.RotationX = saved.Rotation[0];
                    local.RotationY = saved.Rotation[1];
                    local.RotationZ = saved.Rotation[2];
                    local.RotationW = saved.Rotation[3];
                    local.ScaleX = saved.Scale[0];
                    local.ScaleY = saved.Scale[1];
                    local.ScaleZ = saved.Scale[2];
                    transform.Local = local;
                }
                foreach (SceneComponentAsset component in saved.Components)
                    if (AddComponent(actor.ID, component) is not null)
                        components++;
                scene.ComponentOrder[actor.ID.Value] = saved.Components.Select(c => (c.Type, c.Id)).ToList();
                foreach (SceneCompositionAsset composition in saved.Compositions)
                    _pendingOverrides[(actor.ID.Value, composition.Name)] = composition;
            }
            scene.Snapshot = asset.ContentJSON();
            scene.SettleFrames = 1;
            if (!additive)
                WriteState(path);
            Debug.Log($"[Scenes] Opened {path}: {ordered.Count} actor(s), {components} component(s)");
            Changed?.Invoke(scene);
            return true;
        }

        public static bool Save(EditorScene? scene = null)
        {
            scene ??= Active;
            if (scene is null)
                return false;
            if (scene.Path is null)
            {
                SceneDialogs.OpenSaveAs(scene);
                return false;
            }
            return SaveTo(scene, scene.Path, false);
        }

        public static bool SaveAs(EditorScene scene, string path) => SaveTo(scene, path, true);

        private static bool SaveTo(EditorScene scene, string path, bool copy)
        {
            if (Blocked("save the scene") || ProjectService.Assets is not { } assets)
                return false;
            SceneAsset asset = Capture(scene);
            string full = assets.FullPath(path);
            JsonObject meta = CurrentMeta(full) ?? scene.Meta?.DeepClone().AsObject() ?? new JsonObject();
            if (copy && scene.Path is not null && !string.Equals(scene.Path, path, StringComparison.OrdinalIgnoreCase))
                meta = new JsonObject { ["guid"] = AssetMeta.NewGuid(), ["tags"] = meta["tags"]?.DeepClone() ?? new JsonArray() };
            if (meta["guid"] is null)
                meta["guid"] = scene.Guid;
            meta["tags"] ??= new JsonArray();
            JsonObject names = new();
            foreach (string guid in asset.ReferencedGuids().OrderBy(g => g, StringComparer.Ordinal))
                if (assets.PathOf(guid) is string referenced)
                    names[guid] = referenced;
            meta["names"] = names;
            asset.Meta = meta;
            try
            {
                Directory.CreateDirectory(System.IO.Path.GetDirectoryName(full)!);
                asset.Write(full);
            }
            catch (Exception e) when (e is IOException or UnauthorizedAccessException)
            {
                Debug.LogError($"[Scenes] Could not save {path}: {e.Message}");
                return false;
            }
            scene.Path = path;
            scene.Meta = meta;
            scene.Guid = meta["guid"]!.GetValue<string>();
            scene.Snapshot = asset.ContentJSON();
            scene.IsDirty = false;
            assets.Refresh();
            WriteState(path);
            Debug.Log($"[Scenes] Saved {path}: {asset.Actors.Count} actor(s)");
            Changed?.Invoke(scene);
            return true;
        }

        private static JsonObject? CurrentMeta(string full)
        {
            if (!File.Exists(full))
                return null;
            try
            {
                return SceneAsset.Read(full).Meta;
            }
            catch (Exception e) when (e is IOException or JsonException or InvalidDataException)
            {
                return null;
            }
        }

        public static void Close(EditorScene scene)
        {
            DestroyMembers(scene);
            _scenes.Remove(scene);
            if (Active == scene)
                Active = _scenes.FirstOrDefault();
        }

        private static void UnloadAll()
        {
            foreach (EditorScene scene in _scenes.ToArray())
                DestroyMembers(scene);
            _scenes.Clear();
            _pendingOverrides.Clear();
            Active = null;
        }

        private static void DestroyMembers(EditorScene scene)
        {
            foreach (ulong id in scene.Members.Keys.ToArray())
            {
                ActorID actor = new(id);
                if (ActorTree.GetParent(actor) is { } parent && scene.Contains(parent))
                    continue;
                DestroyActor(actor);
            }
            scene.Members.Clear();
            scene.ComponentOrder.Clear();
            scene.ActorExtra.Clear();
            scene.FolderOf.Clear();
            ActorEditorState.ContainersChanged();
        }

        private static void DestroyActor(ActorID actor)
        {
            if (ActorRegistry.TryGet(actor, out Actor? owned))
                owned.Destroy();
        }

        // ------------------------------------------------------------
        // Authoring
        // ------------------------------------------------------------
        // The Hierarchy's selected folder (Hierarchy-only, not in Selection): new root actors go into it.
        public static (EditorScene Scene, SceneFolder Folder)? FolderTarget { get; set; }

        // Runs create with a given folder as the target instead of the Hierarchy's selection.
        public static Actor? CreateInFolder(EditorScene scene, SceneFolder folder, Func<Actor?> create)
        {
            (EditorScene, SceneFolder)? previous = FolderTarget;
            FolderTarget = (scene, folder);
            try
            {
                return create();
            }
            finally
            {
                FolderTarget = previous;
            }
        }

        public static Actor? CreateActor(string name, ActorID? parent = null)
        {
            if (Blocked("add scene actors"))
                return null;
            bool hasParent = parent is { } p0 && p0.Value != 0;
            (EditorScene Scene, SceneFolder Folder)? folder = hasParent ? null : FolderTarget;
            if (folder is { } f && (!Scenes.Contains(f.Scene) || f.Scene.FindFolder(f.Folder.Id) is null))
                folder = null;
            EditorScene scene = (parent is { } p ? SceneOf(p) : null) ?? folder?.Scene ?? Active ?? NewUntitled();
            if (scene.Locked || (hasParent && ActorEditorState.IsLockedInHierarchy(parent!.Value)) || (folder is { } lf && IsFolderLocked(scene, lf.Folder.Id)))
            {
                Debug.LogWarning($"[Scenes] {scene.Name}, the group or the parent is locked; unlock it to add actors");
                return null;
            }
            Actor actor = Actor.Create(name);
            if (hasParent)
                ActorTree.SetParent(actor.ID, parent!.Value, false);
            scene.Members[actor.ID.Value] = SceneAsset.NewActorId();
            if (folder is { } into)
            {
                scene.FolderOf[actor.ID.Value] = into.Folder.Id;
                ContainerEdited(scene);
            }
            Touch(scene);
            Debug.Log($"[Scenes] Created {name} in {scene.Name}");
            return actor;
        }

        // An actor drawing a composition object: Transform + ObjectDriverComponent, named after the object.
        public static Actor? CreateFromObject(string objectPath, ActorID? parent = null)
        {
            if (ProjectService.Assets?.GuidOf(objectPath) is not string guid)
                return null;
            Actor? actor = CreateActor(AssetNames.DisplayName(objectPath), parent);
            if (actor is null)
                return null;
            actor.AddTransform();
            ObjectDriverComponent driver = actor.AddComponent<ObjectDriverComponent>();
            driver.Object = AssetReference.FromText(guid);
            Touch(SceneOf(actor.ID));
            return actor;
        }

        // A camera actor: Transform + Camera (main, perspective), named "Camera".
        public static Actor? CreateCamera(ActorID? parent = null)
        {
            Actor? actor = CreateActor("Camera", parent);
            if (actor is null)
                return null;
            actor.AddTransform();
            actor.AddComponent<Camera>();
            Touch(SceneOf(actor.ID));
            return actor;
        }

        public static bool Destroy(ActorID actor)
        {
            if (Blocked("delete scene actors"))
                return false;
            EditorScene? scene = SceneOf(actor);
            if (scene is null || ObjectDriverComponent.IsCompositionActor(actor))
            {
                Debug.LogWarning($"[Scenes] {ActorTree.GetName(actor)} isn't a scene actor, so it can't be deleted here");
                return false;
            }
            if (ActorEditorState.IsLockedInSubtree(actor))
            {
                Debug.LogWarning($"[Scenes] {ActorTree.GetName(actor)} is locked or has a locked child; unlock it to delete it");
                return false;
            }
            DestroyActor(actor);
            Prune(scene);
            Touch(scene);
            return true;
        }

        public static bool Rename(ActorID actor, string name)
        {
            if (Blocked("rename scene actors") || SceneOf(actor) is not { } scene || name.Trim().Length == 0 || ActorEditorState.IsLockedInHierarchy(actor))
                return false;
            if (!ActorRegistry.TryGet(actor, out Actor? owned))
                return false;
            owned.Name = name.Trim();
            Touch(scene);
            return true;
        }

        private static EditorScene NewUntitled()
        {
            EditorScene scene = new(null, AssetMeta.NewGuid());
            _scenes.Add(scene);
            Active = scene;
            return scene;
        }

        private static void Prune(EditorScene scene)
        {
            foreach (ulong id in scene.Members.Keys.ToArray())
                if (!ActorRegistry.TryGet(new ActorID(id), out _))
                {
                    scene.Members.Remove(id);
                    scene.ComponentOrder.Remove(id);
                    scene.ActorExtra.Remove(id);
                    scene.FolderOf.Remove(id);
                }
        }

        // ------------------------------------------------------------
        // Editor flags and folders
        // ------------------------------------------------------------
        // The scene an actor's tree belongs to (Composition Actors through their driver).
        public static EditorScene? SceneOfTree(ActorID actor)
        {
            ActorID current = actor;
            for (int guard = 0; guard < 256; guard++)
            {
                if (SceneOf(current) is { } scene)
                    return scene;
                if (ActorTree.GetParent(current) is not { } parent || parent.Value == 0)
                    return null;
                current = parent;
            }
            return null;
        }

        private static (bool Hidden, bool Locked) ContainerState(ActorID root) =>
            SceneOf(root) is { } scene ? scene.FolderState(scene.FolderIdOf(root)) : (false, false);

        private static void OnEditorStateChanged(IReadOnlyList<ActorID> actors)
        {
            foreach (ActorID actor in actors)
                Touch(SceneOfTree(actor));
        }

        private static void ContainerEdited(EditorScene scene)
        {
            ActorEditorState.ContainersChanged();
            Touch(scene);
        }

        // Sends the effectively hidden actors to the render systems when the set changes.
        private static void PushHidden()
        {
            bool any = ActorEditorState.AnyHidden || _scenes.Any(s => s.Hidden || s.Folders.Any(f => f.Hidden));
            if (!any && _pushedHidden.Count == 0)
                return;
            _hidden.Clear();
            if (any)
            {
                List<ActorID> roots = new();
                ActorTree.GetRoots(roots);
                ActorEditorState.CollectHidden(roots, _hidden);
            }
            if (_hidden.SequenceEqual(_pushedHidden))
                return;
            _pushedHidden.Clear();
            _pushedHidden.AddRange(_hidden);
            MeshRenderer.SetEditorHidden(System.Runtime.InteropServices.CollectionsMarshal.AsSpan(_hidden));
        }

        public static int HiddenCount => _pushedHidden.Count;

        public static void SetSceneHidden(EditorScene scene, bool hidden)
        {
            if (scene.Hidden == hidden)
                return;
            scene.Hidden = hidden;
            ContainerEdited(scene);
        }

        public static void SetSceneLocked(EditorScene scene, bool locked)
        {
            if (scene.Locked == locked)
                return;
            scene.Locked = locked;
            ContainerEdited(scene);
        }

        public static void SetFolderHidden(EditorScene scene, SceneFolder folder, bool hidden)
        {
            if (folder.Hidden == hidden)
                return;
            folder.Hidden = hidden;
            ContainerEdited(scene);
        }

        public static void SetFolderLocked(EditorScene scene, SceneFolder folder, bool locked)
        {
            if (folder.Locked == locked)
                return;
            folder.Locked = locked;
            ContainerEdited(scene);
        }

        // Locked by itself, a parent folder or the scene: no rename, delete or moves in or out.
        public static bool IsFolderLocked(EditorScene scene, string? folderId) => scene.FolderState(folderId).Locked;

        public static SceneFolder? CreateFolder(EditorScene scene, string? parentId, string name = "New Group")
        {
            if (Blocked("add groups"))
                return null;
            if (IsFolderLocked(scene, parentId))
            {
                Debug.LogWarning($"[Scenes] {scene.Name} or the group is locked; unlock it to add groups");
                return null;
            }
            SceneFolder folder = new() { Id = AssetMeta.NewGuid(), Name = name, Parent = scene.FindFolder(parentId)?.Id ?? string.Empty };
            scene.Folders.Add(folder);
            ContainerEdited(scene);
            Debug.Log($"[Scenes] Created group {name} in {scene.Name}");
            return folder;
        }

        public static bool RenameFolder(EditorScene scene, SceneFolder folder, string name)
        {
            if (Blocked("rename groups") || name.Trim().Length == 0 || IsFolderLocked(scene, folder.Id))
                return false;
            folder.Name = name.Trim();
            Touch(scene);
            return true;
        }

        // Removes the folder; its folders and actors move to its parent.
        public static bool DeleteFolder(EditorScene scene, SceneFolder folder)
        {
            if (Blocked("delete groups") || IsFolderLocked(scene, folder.Id))
                return false;
            foreach (SceneFolder child in scene.Folders.Where(f => string.Equals(f.Parent, folder.Id, StringComparison.OrdinalIgnoreCase)))
                child.Parent = folder.Parent;
            foreach ((ulong actor, string id) in scene.FolderOf.ToArray())
                if (string.Equals(id, folder.Id, StringComparison.OrdinalIgnoreCase))
                {
                    if (folder.Parent.Length > 0)
                        scene.FolderOf[actor] = folder.Parent;
                    else
                        scene.FolderOf.Remove(actor);
                }
            scene.Folders.Remove(folder);
            ContainerEdited(scene);
            return true;
        }

        // Moves a root member into a folder of its scene (null or empty = the scene root).
        public static bool MoveToFolder(ActorID actor, string? folderId)
        {
            if (Blocked("move actors") || SceneOf(actor) is not { } scene)
                return false;
            if (ActorTree.GetParent(actor) is { } parent && parent.Value != 0)
                return false;
            string? from = scene.FolderIdOf(actor);
            SceneFolder? target = scene.FindFolder(folderId);
            if (string.Equals(from, target?.Id, StringComparison.OrdinalIgnoreCase))
                return false;
            if (ActorEditorState.IsLockedInHierarchy(actor) || (target is not null && IsFolderLocked(scene, target.Id)) || scene.Locked)
            {
                Debug.LogWarning($"[Scenes] {ActorTree.GetName(actor)}: a locked actor, group or scene can't move");
                return false;
            }
            if (target is null)
                scene.FolderOf.Remove(actor.Value);
            else
                scene.FolderOf[actor.Value] = target.Id;
            ContainerEdited(scene);
            return true;
        }

        // Moves a folder under another folder of the same scene (null = the scene root).
        public static bool MoveFolder(EditorScene scene, SceneFolder folder, string? parentId)
        {
            SceneFolder? target = scene.FindFolder(parentId);
            if (Blocked("move groups") || IsFolderLocked(scene, folder.Id) || (target is not null && (IsFolderLocked(scene, target.Id) || scene.IsInFolder(target.Id, folder.Id))))
                return false;
            folder.Parent = target?.Id ?? string.Empty;
            ContainerEdited(scene);
            return true;
        }

        // ------------------------------------------------------------
        // Capture
        // ------------------------------------------------------------
        public static SceneAsset Capture(EditorScene scene)
        {
            Prune(scene);
            SceneAsset asset = new() { Meta = scene.Meta, Extra = scene.Extra.DeepClone().AsObject(), Hidden = scene.Hidden, Locked = scene.Locked };
            foreach (SceneFolder folder in scene.Folders)
                asset.Folders.Add(new SceneFolderAsset { Id = folder.Id, Name = folder.Name, Parent = folder.Parent, Hidden = folder.Hidden, Locked = folder.Locked });
            List<ActorID> roots = new();
            ActorTree.GetRoots(roots);
            foreach (ActorID root in roots)
                CaptureTree(scene, asset, root, string.Empty);
            return asset;
        }

        private static void CaptureTree(EditorScene scene, SceneAsset asset, ActorID actor, string parentId)
        {
            string childParent = parentId;
            if (scene.IdOf(actor) is string id && !ObjectDriverComponent.IsCompositionActor(actor))
            {
                asset.Actors.Add(CaptureActor(scene, actor, id, parentId));
                childParent = id;
            }
            List<ActorID> children = new();
            ActorTree.GetChildren(actor, children);
            foreach (ActorID child in children)
                CaptureTree(scene, asset, child, childParent);
        }

        private static SceneActorAsset CaptureActor(EditorScene scene, ActorID actor, string id, string parentId)
        {
            SceneActorAsset saved = new()
            {
                Id = id, Name = ActorTree.GetName(actor), Parent = parentId,
                Folder = parentId.Length == 0 ? scene.FolderIdOf(actor) ?? string.Empty : string.Empty,
                Hidden = ActorEditorState.IsHidden(actor), Locked = ActorEditorState.IsLocked(actor),
            };
            if (scene.ActorExtra.TryGetValue(actor.Value, out JsonObject? extra))
                saved.Extra = extra.DeepClone().AsObject();
            if (ActorRegistry.TryGet(actor, out Actor? owned) && owned.Transform is Transform transform && transform.IsValid)
            {
                TransformData local = transform.Local;
                saved.HasTransform = true;
                saved.Position = new[] { local.PositionX, local.PositionY, local.PositionZ };
                saved.Rotation = new[] { local.RotationX, local.RotationY, local.RotationZ, local.RotationW };
                saved.Scale = new[] { local.ScaleX, local.ScaleY, local.ScaleZ };
            }
            List<object> components = ComponentsOf(actor);
            scene.ComponentOrder.TryGetValue(actor.Value, out List<(string Type, string Id)>? known);
            List<string> ids = ComponentIds.Bind(components, known);
            for (int i = 0; i < components.Count; i++)
            {
                saved.Components.Add(CaptureComponent(components[i], ids[i]));
                if (components[i].GetType().FullName == "xg.ObjectDriverComponent")
                    CaptureCompositions(actor, components[i], saved);
            }
            scene.ComponentOrder[actor.Value] = saved.Components.Select(c => (c.Type, c.Id)).ToList();
            return saved;
        }

        private static List<object> ComponentsOf(ActorID actor)
        {
            _components.Clear();
            ActorTree.GetComponents(actor, _components);
            return _components.Where(c => c.GetType().FullName != typeof(Transform).FullName).ToList();
        }

        private static SceneComponentAsset CaptureComponent(object component, string id)
        {
            SceneComponentAsset saved = new() { Id = id, Type = TypeBridge.KeyOf(component.GetType()), Fields = ReloadState.CaptureFields(component) };
            int count = DataCount(component);
            for (int i = 0; i < count; i++)
                if (GetData(component, i) is object data)
                    saved.Data.Add(ReloadState.CaptureFields(data));
            return saved;
        }

        // A driver's Composition Actors: components that differ from the .xco's authored defaults (by component id).
        private static void CaptureCompositions(ActorID driver, object component, SceneActorAsset saved)
        {
            if (component.GetType().GetProperty("Object")?.GetValue(component)?.ToString() is not { Length: > 0 } guid
                || ProjectService.Assets is not { } assets || assets.PathOf(guid) is not string path)
                return;
            ObjectAsset objectAsset;
            try
            {
                objectAsset = ObjectAsset.Read(assets.FullPath(path));
            }
            catch (Exception e) when (e is IOException or JsonException or InvalidDataException)
            {
                return;
            }
            foreach (ObjectNodeAsset node in objectAsset.Objects.Where(o => o.Expose == ObjectNodeExpose.Actor))
            {
                ActorID composed = ObjectDriverComponent.GetCompositionActor(driver, node.Name);
                if (composed.Value == 0)
                {
                    if (_pendingOverrides.TryGetValue((driver.Value, node.Name), out SceneCompositionAsset? waiting))
                        saved.Compositions.Add(waiting.Clone());
                    continue;
                }
                List<(string Type, string Id, JsonObject Fields)> authored = (node.Components?.OfType<JsonObject>() ?? Enumerable.Empty<JsonObject>())
                    .Select(e => (e["type"]?.GetValue<string>() ?? string.Empty, e["id"]?.GetValue<string>() ?? string.Empty, e["fields"] as JsonObject ?? new JsonObject()))
                    .ToList();
                List<object> components = ComponentsOf(composed);
                List<string> ids = ComponentIds.Bind(components, authored.Select(a => (a.Type, a.Id)).ToList());
                SceneCompositionAsset entry = new() { Node = node.Id, Name = node.Name, Hidden = ActorEditorState.IsHidden(composed) };
                for (int i = 0; i < components.Count; i++)
                {
                    SceneComponentAsset current = CaptureComponent(components[i], ids[i]);
                    var match = authored.FirstOrDefault(a => string.Equals(a.Id, ids[i], StringComparison.OrdinalIgnoreCase));
                    if (match.Id is { Length: > 0 })
                    {
                        current.Fields = ChangedFields(current.Fields, Baseline(components[i].GetType(), match.Fields));
                        if (current.Fields.Count == 0 && current.Data.Count == 0)
                            continue;
                    }
                    entry.Components.Add(current);
                }
                if (entry.Components.Count > 0 || entry.Hidden)
                    saved.Compositions.Add(entry);
            }
        }

        // The values an authored component starts with: a fresh instance with the authored fields applied.
        private static JsonObject Baseline(Type type, JsonObject authored)
        {
            try
            {
                object instance = Activator.CreateInstance(type)!;
                ReloadState.ApplyFields(instance, authored);
                return ReloadState.CaptureFields(instance);
            }
            catch (Exception e) when (e is MissingMethodException or TargetInvocationException or MemberAccessException)
            {
                return authored;
            }
        }

        // Only the fields whose value differs from the baseline (an override keeps authored defaults flowing).
        private static JsonObject ChangedFields(JsonObject current, JsonObject baseline)
        {
            JsonObject changed = new();
            foreach ((string name, JsonNode? value) in current)
                if (baseline[name]?.ToJsonString() != value?.ToJsonString())
                    changed[name] = value?.DeepClone();
            return changed;
        }

        // ------------------------------------------------------------
        // Load helpers
        // ------------------------------------------------------------
        private static object? AddComponent(ActorID actor, SceneComponentAsset saved)
        {
            if (!ActorTree.AddComponent(actor, saved.Type))
            {
                Debug.LogWarning($"[Scenes] {ActorTree.GetName(actor)}: no module could add {saved.Type}");
                return null;
            }
            string fullName = saved.Type.Split(',')[0].Trim();
            object? component = ComponentsOf(actor).LastOrDefault(c => c.GetType().FullName == fullName && ComponentIds.Get(c) is null)
                ?? ComponentsOf(actor).LastOrDefault(c => c.GetType().FullName == fullName);
            if (component is null)
                return null;
            ApplyComponent(actor, component, saved);
            return component;
        }

        private static void ApplyComponent(ActorID actor, object component, SceneComponentAsset saved)
        {
            ComponentIds.Set(component, saved.Id);
            foreach (string skipped in ReloadState.ApplyFields(component, saved.Fields))
                Debug.LogWarning($"[Scenes] {ActorTree.GetName(actor)}: not applied: {skipped}");
            for (int i = 0; i < saved.Data.Count && i < DataCount(component); i++)
            {
                if (GetData(component, i) is not object data)
                    continue;
                foreach (string skipped in ReloadState.ApplyFields(data, saved.Data[i]))
                    Debug.LogWarning($"[Scenes] {ActorTree.GetName(actor)}: not applied: {skipped}");
                SetData(component, i, data);
            }
        }

        private static void OnCompositionSpawned(ActorID actor)
        {
            ActorID owner = ObjectDriverComponent.CompositionOwner(actor);
            if (!_pendingOverrides.Remove((owner.Value, ActorTree.GetName(actor)), out SceneCompositionAsset? overrides))
                return;
            if (overrides.Hidden)
                ActorEditorState.Restore(actor, true, false);
            int applied = 0;
            foreach (SceneComponentAsset saved in overrides.Components)
            {
                object? existing = ComponentsOf(actor).FirstOrDefault(c => string.Equals(ComponentIds.Get(c), saved.Id, StringComparison.OrdinalIgnoreCase));
                if (existing is not null)
                    ApplyComponent(actor, existing, saved);
                else if (AddComponent(actor, saved) is null)
                    continue;
                applied++;
            }
            Debug.Log($"[Scenes] Applied {applied} instance override(s) to {ActorTree.GetName(actor)}");
        }

        private static int DataCount(object component) =>
            IsDataSet(component.GetType()) && component.GetType().GetProperty("DataCount")?.GetValue(component) is int count ? count : 0;

        private static object? GetData(object component, int index) =>
            component.GetType().GetMethod("GetData", new[] { typeof(int) })?.Invoke(component, new object[] { index });

        private static void SetData(object component, int index, object value) =>
            component.GetType().GetMethod("SetData", new[] { typeof(int), typeof(object) })?.Invoke(component, new[] { index, value });

        private static bool IsDataSet(Type? type)
        {
            for (; type is not null; type = type.BaseType)
                if (type.FullName == "xg.ComponentDataSet")
                    return true;
            return false;
        }

        // ------------------------------------------------------------
        // Editor state (Library/EditorState.json, per user)
        // ------------------------------------------------------------
        private static string? StatePath() =>
            ProjectService.Current is { } project ? System.IO.Path.Combine(project.Root, "Library", StateFile) : null;

        private static JsonObject? ReadState()
        {
            if (StatePath() is not string path || !File.Exists(path))
                return null;
            try
            {
                return JsonNode.Parse(File.ReadAllText(path)) as JsonObject;
            }
            catch (Exception e) when (e is IOException or JsonException)
            {
                return null;
            }
        }

        private static void WriteState(string lastScene)
        {
            if (StatePath() is not string path)
                return;
            JsonObject state = ReadState() ?? new JsonObject();
            state["lastScene"] = lastScene;
            try
            {
                Directory.CreateDirectory(System.IO.Path.GetDirectoryName(path)!);
                JSONFile.WriteText(path, state.ToJsonString(JSONFile.Options) + "\r\n");
            }
            catch (Exception e) when (e is IOException or UnauthorizedAccessException)
            {
                Debug.LogWarning($"[Scenes] Could not write {path}: {e.Message}");
            }
        }
    }
}
