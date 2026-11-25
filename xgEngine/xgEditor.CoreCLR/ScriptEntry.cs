// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
﻿using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.Json;
using xg;
using xg.math;

namespace xgEditor.CoreCLR
{
    // Test hook: a managed subsystem registered through xg.Subsystem. Logs its lifecycle.
    internal sealed class SampleSubsystem : ISubsystem
    {
        public string Name { get; set; } = "sample";

        public void Initialize() => Debug.Log($"[Debug] Subsystem {Name}: initialized");
        public void Update(float dt) { }
        public void Shutdown() => Debug.Log($"[Debug] Subsystem {Name}: shutdown");
    }

    // Test hook (XG_RESOURCE_TEST_TEXTURE=<asset path>): builds the image asset, acquires its texture through the resource
    // system by breadcrumb <asset guid>/<texture item guid> (loaded on a job worker, uploaded on the main thread) and puts
    // it on the Scene panel's test cube.
    internal static class TextureSmoke
    {
        static ResourceHandle _handle;
        static bool _waiting;
        static int _frames;

        public static void Init()
        {
            string? path = Environment.GetEnvironmentVariable("XG_RESOURCE_TEST_TEXTURE");
            if (string.IsNullOrEmpty(path) || xgEditor.CoreCLR.ProjectService.Assets is not { } assets)
                return;
            assets.BuildAll();
            string? guid = assets.GuidOf(path);
            xg.editor.AssetMeta meta = xg.editor.AssetMeta.Read(assets.FullPath(xg.editor.AssetMeta.PathFor(path)));
            xg.editor.AssetItem? item = meta.Items.FirstOrDefault(i => i.Kind == xg.editor.TextureAssets.ItemKind && !i.Missing);
            uint type = Resources.FindType(Resources.TextureType);
            if (guid is null || item is null || type == 0)
            {
                Debug.LogWarning($"[Textures] test: no texture for {path} (guid {guid ?? "none"}, item {item?.Guid ?? "none"}, type {type})");
                return;
            }
            ResourceID id = ResourceID.Parse($"{guid}/{item.Guid}");
            _handle = Resources.Acquire(type, id);
            _waiting = true;
            Debug.Log($"[Textures] test: acquired {id} for {path}, state {_handle.State}");
        }

        public static void Update()
        {
            if (!_waiting)
                return;
            ++_frames;
            ResourceState state = _handle.State;
            if (state == ResourceState.Pending)
                return;
            _waiting = false;
            if (state != ResourceState.Ready || !Resources.GetTexture(_handle, out TextureInfo info))
            {
                Debug.LogWarning($"[Textures] test: {state} after {_frames} frame(s)");
                return;
            }
            Debug.Log($"[Textures] test: Ready after {_frames} frame(s): {info.Width}x{info.Height}, {info.MipCount} mip(s), {info.Format}, usage {info.Usage}, sRGB {(info.Flags & 1) != 0}, texture id {info.TextureId}");
            foreach (ScenePanel panel in xg.imui.ImUI.RegisteredPanels.OfType<ScenePanel>())
                panel.SetTexture(info.TextureId, (int)Math.Min(info.Usage, 2u));
        }
    }

    // Test hook (XG_RESOURCE_TEST_MODEL=<asset path>): builds the mesh asset, acquires its "model" resource by the asset
    // GUID (loaded on a job worker, GPU meshes created on the main thread) and draws it next to the Scene panel's test
    // cube, scaled to about 2 units tall and standing on the grid at x = -2.
    // Test hook (XG_TRANSFORM_TEST=1): native transforms through the managed proxy. Parent/child world poses, keep-world
    // reparenting, an actor without a transform in the middle, change versions, and destroying a parent's subtree.
    // XG_TRANSFORM_TEST=play (with XG_UI_TEST_PLAY=play,stop): transforms changed, added or removed during play are put back
    // at Stop. XG_TRANSFORM_TEST_INSPECT=<actor name>: gives that actor a posed transform (for the Inspector card).
    internal static class TransformSmoke
    {
        static int _failures;
        static Actor? _playActor;
        static Actor? _playAdded;
        static Actor? _playRemoved;
        static bool _playChanged;
        static bool _playChecked;
        static string? _inspectTarget = Environment.GetEnvironmentVariable("XG_TRANSFORM_TEST_INSPECT");
        static int _frames;

        static void Check(string label, Vector3 actual, float x, float y, float z)
        {
            bool ok = MathF.Abs(actual.x - x) < 1e-3f && MathF.Abs(actual.y - y) < 1e-3f && MathF.Abs(actual.z - z) < 1e-3f;
            if (!ok)
                _failures++;
            Debug.Log($"[Transforms] {(ok ? "PASS" : "FAIL")} {label}: ({actual.x:F3}, {actual.y:F3}, {actual.z:F3}) expected ({x:F3}, {y:F3}, {z:F3})");
        }

        static void Check(string label, bool ok)
        {
            if (!ok)
                _failures++;
            Debug.Log($"[Transforms] {(ok ? "PASS" : "FAIL")} {label}");
        }

        public static void Init()
        {
            string? mode = Environment.GetEnvironmentVariable("XG_TRANSFORM_TEST");
            if (mode == "play")
            {
                _playActor = Actor.Create("TransformTest Play");
                _playActor.AddTransform().LocalPosition = new Vector3(1, 2, 3);
                _playAdded = Actor.Create("TransformTest Play Added");
                _playRemoved = Actor.Create("TransformTest Play Removed");
                _playRemoved.AddTransform().LocalPosition = new Vector3(4, 5, 6);
                return;
            }
            if (mode != "1")
                return;
            try
            {
                Run();
            }
            catch (Exception e)
            {
                _failures++;
                Debug.LogException(e);
            }
            Debug.Log($"[Transforms] done, {_failures} failure(s)");
        }

        public static void Update()
        {
            _frames++;
            if (_inspectTarget is not null && _frames > 10)
            {
                foreach (Actor actor in ActorRegistry.Actors)
                {
                    if (actor.Name != _inspectTarget)
                        continue;
                    Transform t = actor.AddTransform();
                    t.SetLocal(new Vector3(1.5f, 0.25f, -3), Transform.FromEuler(new Vector3(10, 45, 0)), new Vector3(1, 1, 1));
                    Debug.Log($"[Transforms] gave '{_inspectTarget}' a transform, euler {t.LocalEulerAngles}");
                    _inspectTarget = null;
                    break;
                }
            }
            if (_playActor is null || _playChecked)
                return;
            if (!_playChanged && PlayMode.State == PlayState.Playing)
            {
                _playActor.Transform!.LocalPosition = new Vector3(9, 9, 9);
                _playAdded!.AddTransform();
                _playRemoved!.RemoveTransform();
                _playChanged = true;
                Debug.Log("[Transforms] changed transforms during play");
            }
            else if (_playChanged && PlayMode.State == PlayState.Edit)
            {
                _playChecked = true;
                Check("play: moved transform restored", _playActor.Transform!.LocalPosition, 1, 2, 3);
                Check("play: transform added in play removed", _playAdded!.Transform is null);
                Check("play: transform removed in play re-added", _playRemoved!.Transform is { } r && MathF.Abs(r.LocalPosition.y - 5) < 1e-3f);
                Vector3 e = Transform.ToEuler(Transform.FromEuler(new Vector3(30, -60, 15)));
                Check("euler round trip", e, 30, -60, 15);
                Debug.Log($"[Transforms] play done, {_failures} failure(s)");
            }
        }

        static void Run()
        {
            Actor parent = Actor.Create("TransformTest Parent");
            Actor child = Actor.Create("TransformTest Child");
            child.Parent = parent;
            Check("no transform by default", parent.Transform is null && child.GetComponent<Transform>() is null);

            Transform pt = parent.AddTransform();
            Transform ct = (Transform)child.AddComponent(typeof(Transform));
            Check("identity local", ct.LocalPosition.x == 0 && ct.LocalScale.y == 1 && ct.LocalRotation.w == 1);

            pt.LocalPosition = new Vector3(10, 0, 0);
            ct.LocalPosition = new Vector3(1, 2, 3);
            Check("child world = parent + local", ct.Position, 11, 2, 3);

            float h = MathF.Sqrt(0.5f);
            pt.LocalRotation = new Quaternion(0, h, 0, h);
            Check("parent yaw 90 rotates child", ct.Position, 13, 2, -1);

            ulong before = Transform.CurrentVersion;
            pt.LocalScale = new Vector3(2, 2, 2);
            List<ActorID> changed = new();
            Transform.CollectChanged(before, changed);
            Check($"changed after scale ({changed.Count})", changed.Count >= 1);
            Check("parent scale 2", ct.Position, 16, 4, -2);
            pt.LocalScale = new Vector3(1, 1, 1);

            Check("reparent to root keeps world", ct.SetParent(null));
            Check("world kept", ct.Position, 13, 2, -1);
            Check("local = world at root", ct.LocalPosition, 13, 2, -1);

            Check("reparent keepWorld=false", ct.SetParent(pt, keepWorld: false));
            Check("local kept", ct.LocalPosition, 13, 2, -1);
            Check("world follows parent", ct.Position, 9, 2, -13);

            Actor mid = Actor.Create("TransformTest Mid");
            mid.Parent = parent;
            Check("reparent under untransformed mid", child.SetParent(mid.ID));
            Check("mid passes parent world through", ct.Position, 9, 2, -13);
            pt.LocalPosition = new Vector3(0, 5, 0);
            Check("parent move reaches grandchild", ct.Position, -1, 7, -13);

            Check("cycle rejected", !parent.SetParent(child.ID));

            child.RemoveTransform();
            Check("transform removed", child.Transform is null && !Transform.Exists(child.ID));

            ActorID childId = child.ID;
            ActorID midId = mid.ID;
            parent.Destroy();
            Check("children destroyed with parent", !child.IsValid && !mid.IsValid);
            Check("native subtree gone", ActorTree.GetName(childId).Length == 0 && ActorTree.GetName(midId).Length == 0);
        }
    }

    public struct SpinData : IComponentData
    {
        public float Angle;
        public float Speed;
        public int Doomed;
    }

    public struct LiftData : IComponentData
    {
        public float Height;
    }

    // Test component for XG_DATA_TEST: two data structs, one plain data update and one that reads the native transform.
    public sealed class DataSpinner : ComponentDataSet<SpinData, LiftData>,
                                      IComponentDataUpdate<DataSpinner, SpinData>,
                                      IComponentDataUpdate<DataSpinner, LiftData, TransformData>
    {
        internal static readonly System.Collections.Concurrent.ConcurrentDictionary<int, byte> Threads = new();
        internal static int InvokeCount;
        internal static bool InvokeOnMain = true;

        public static void OnDataUpdate(ref SpinData data, in DataUpdateContext context)
        {
            Threads.TryAdd(Environment.CurrentManagedThreadId, 0);
            data.Angle += data.Speed * context.DeltaTime;
            if (data.Doomed == 1)
            {
                data.Doomed = 2;
                context.Commands.Destroy(context.Actor);
            }
            if (data.Speed > 1.5f)
                context.Commands.Invoke(() =>
                {
                    InvokeCount++;
                    InvokeOnMain &= ScriptThread.IsMain && !ScriptThread.InDataPass;
                });
        }

        public static void OnDataUpdate(ref LiftData lift, in TransformData transform, in DataUpdateContext context)
        {
            lift.Height = transform.PositionY;
        }
    }

    // Test hook (XG_DATA_TEST=1, with XG_UI_TEST_PLAY=play,frames,stop): 3000 DataSpinner actors (enough for range jobs),
    // every other one with a transform at y = index. In play: angles advance, LiftData follows the transform only where
    // there is one, command buffers destroy flagged actors and run main-thread callbacks; after Stop the data is restored.
    internal static class DataSmoke
    {
        const int Count = 3000;
        static readonly List<Actor> _actors = new();
        static int _playFrames;
        static bool _checkedPlay;
        static bool _checkedStop;
        static int _failures;

        static void Check(string label, bool ok)
        {
            if (!ok)
                _failures++;
            Debug.Log($"[Data] {(ok ? "PASS" : "FAIL")} {label}");
        }

        public static void Init()
        {
            if (Environment.GetEnvironmentVariable("XG_DATA_TEST") != "1")
                return;
            for (int i = 0; i < Count; i++)
            {
                Actor actor = Actor.Create($"DataTest {i}");
                DataSpinner spinner = actor.AddComponent<DataSpinner>();
                ref SpinData spin = ref spinner.Data1;
                spin.Speed = i == 0 ? 2.0f : 1.0f;
                spin.Doomed = i >= Count - 10 ? 1 : 0;
                if (i % 2 == 0)
                    actor.AddTransform().LocalPosition = new Vector3(0, i, 0);
                _actors.Add(actor);
            }
            ComponentData.All<SpinData>(out ReadOnlySpan<ActorID> owners);
            Check($"pool holds {owners.Length} SpinData", owners.Length == Count);
            DataSpinner shared = _actors[1].AddComponent<DataSpinner>();
            Check("second component shares the element", Unsafe.AreSame(ref shared.Data1, ref _actors[1].GetComponent<DataSpinner>()!.Data1));
            _actors[1].RemoveComponent<DataSpinner>();
            Check("element kept while one component holds it", ComponentData.Has<SpinData>(_actors[1].ID));
        }

        public static void Update()
        {
            if (_actors.Count == 0)
                return;
            if (PlayMode.State == PlayState.Playing && !_checkedPlay && ++_playFrames == 30)
            {
                _checkedPlay = true;
                float angle = _actors[1].GetComponent<DataSpinner>()!.Data1.Angle;
                Check($"angle advanced ({angle:F3})", angle > 0.05f);
                Check($"speed 2 advances twice as fast ({_actors[0].GetComponent<DataSpinner>()!.Data1.Angle:F3})",
                    MathF.Abs(_actors[0].GetComponent<DataSpinner>()!.Data1.Angle - 2 * angle) < 1e-3f);
                Check($"transformed lift = y ({_actors[100].GetComponent<DataSpinner>()!.Data2.Height})", _actors[100].GetComponent<DataSpinner>()!.Data2.Height == 100);
                Check("untransformed lift skipped", _actors[101].GetComponent<DataSpinner>()!.Data2.Height == 0);
                Check($"ran on {DataSpinner.Threads.Count} thread(s)", DataSpinner.Threads.Count > 1);
                Check($"command callbacks ran on main thread ({DataSpinner.InvokeCount})", DataSpinner.InvokeCount > 0 && DataSpinner.InvokeOnMain);
                int doomedAlive = _actors.Skip(Count - 10).Count(a => a.IsValid);
                ComponentData.All<SpinData>(out ReadOnlySpan<ActorID> owners);
                Check($"destroyed by command buffer ({doomedAlive} alive, pool {owners.Length})", doomedAlive == 0 && owners.Length == Count - 10);
            }
            if (_checkedPlay && !_checkedStop && PlayMode.State == PlayState.Edit)
            {
                _checkedStop = true;
                Check($"angle restored after Stop ({_actors[1].GetComponent<DataSpinner>()!.Data1.Angle})", _actors[1].GetComponent<DataSpinner>()!.Data1.Angle == 0);
                Debug.Log($"[Data] done, {_failures} failure(s)");
            }
        }
    }

    internal static unsafe class ModelSmoke
    {
        static ResourceHandle _handle;
        static bool _waiting;
        static int _frames;

        public static void Init()
        {
            string? path = Environment.GetEnvironmentVariable("XG_RESOURCE_TEST_MODEL");
            if (string.IsNullOrEmpty(path) || xgEditor.CoreCLR.ProjectService.Assets is not { } assets)
                return;
            System.Diagnostics.Stopwatch clock = System.Diagnostics.Stopwatch.StartNew();
            assets.BuildAll();
            string? guid = assets.GuidOf(path);
            uint type = Resources.FindType(Resources.ModelType);
            xg.editor.AssetBuildManifest? manifest = guid is null ? null : assets.Pipeline.ManifestOf(guid);
            Debug.Log($"[Models] test: built {path} in {clock.ElapsedMilliseconds} ms: {(manifest?.Error ?? string.Join(" ", manifest?.Tags ?? new List<string>()))}");
            if (guid is null || type == 0 || manifest is null || manifest.Error is not null)
            {
                Debug.LogWarning($"[Models] test: no model for {path} (guid {guid ?? "none"}, type {type})");
                return;
            }
            ResourceID id = ResourceID.Parse(guid);
            _handle = Resources.Acquire(type, id);
            _waiting = true;
            Debug.Log($"[Models] test: acquired {id}, state {_handle.State}");
        }

        public static void Update()
        {
            if (!_waiting)
                return;
            ++_frames;
            ResourceState state = _handle.State;
            if (state == ResourceState.Pending)
                return;
            _waiting = false;
            if (state != ResourceState.Ready || !Resources.GetModel(_handle, out ModelInfo info))
            {
                Debug.LogWarning($"[Models] test: {state} after {_frames} frame(s)");
                return;
            }
            float height = info.BoundsMax[1] - info.BoundsMin[1];
            float extent = Math.Max(height, Math.Max(info.BoundsMax[0] - info.BoundsMin[0], info.BoundsMax[2] - info.BoundsMin[2]));
            float scale = extent > 0.0f ? 2.0f / extent : 1.0f;
            float[] place =
            {
                scale, 0, 0, 0,
                0, scale, 0, 0,
                0, 0, scale, 0,
                -2.0f - (info.BoundsMin[0] + info.BoundsMax[0]) * 0.5f * scale, -info.BoundsMin[1] * scale, -(info.BoundsMin[2] + info.BoundsMax[2]) * 0.5f * scale, 1,
            };
            SceneViews.Draw[] draws = new SceneViews.Draw[info.PartCount];
            long triangles = 0;
            for (int i = 0; i < info.PartCount; i++)
            {
                if (!Resources.GetModelPart(_handle, i, out ModelPartInfo part))
                    continue;
                triangles += part.TriangleCount;
                draws[i].Mesh = part.Mesh;
                for (int row = 0; row < 4; row++)
                    for (int column = 0; column < 4; column++)
                    {
                        float sum = 0.0f;
                        for (int k = 0; k < 4; k++)
                            sum += part.World[row * 4 + k] * place[k * 4 + column];
                        draws[i].World[row * 4 + column] = sum;
                    }
            }
            Debug.Log($"[Models] test: Ready after {_frames} frame(s): {info.PartCount} part(s), {triangles} triangle(s), {info.NodeCount} node(s), " +
                $"bounds {info.BoundsMin[0]:0.##},{info.BoundsMin[1]:0.##},{info.BoundsMin[2]:0.##} .. {info.BoundsMax[0]:0.##},{info.BoundsMax[1]:0.##},{info.BoundsMax[2]:0.##}");
            foreach (ScenePanel panel in xg.imui.ImUI.RegisteredPanels.OfType<ScenePanel>())
                panel.SetDraws(draws);
        }
    }

    // Test hook (XG_RESOURCE_TEST_MATERIAL=<model path>:<material path>[:edit=<param>=<x,y,z,w>]): builds both assets,
    // creates "MeshRenderer Test" (Transform + MeshRenderer) at x = +2, two units tall, and logs [Draw] lines; edit=
    // rewrites the material after 120 frames to show the rebuilt material in the views.
    internal static unsafe class MaterialSmoke
    {
        static Actor? _actor;
        static ResourceHandle _model;
        static bool _placed;
        static int _frames;
        static string? _materialPath;
        static string? _edit;

        public static void Init()
        {
            string? spec = Environment.GetEnvironmentVariable("XG_RESOURCE_TEST_MATERIAL");
            if (string.IsNullOrEmpty(spec) || xgEditor.CoreCLR.ProjectService.Assets is not { } assets)
                return;
            string[] parts = spec.Split(':');
            if (parts.Length < 2)
                return;
            _materialPath = parts[1];
            _edit = parts.Length > 2 && parts[2].StartsWith("edit=", StringComparison.Ordinal) ? parts[2][5..] : null;
            assets.BuildAll();
            foreach (string path in new[] { parts[0], parts[1] })
            {
                string? guid = assets.GuidOf(path);
                xg.editor.AssetBuildManifest? manifest = guid is null ? null : assets.Pipeline.ManifestOf(guid);
                Debug.Log($"[Draw] test: {path}: {(guid is null ? "no asset" : manifest?.Error ?? string.Join(" ", manifest?.Tags ?? new List<string>()))}");
            }
            if (xg.editor.MaterialAsset.IsMaterial(parts[1]) && assets.GuidOf(parts[1]) is string materialGuid)
            {
                xg.editor.MaterialAsset material = xg.editor.MaterialAsset.Read(assets.FullPath(parts[1]));
                string? shaderPath = assets.PathOf(material.Shader);
                if (shaderPath is not null && assets.Pipeline.ManifestOf(material.Shader) is { } shaderManifest)
                    Debug.Log($"[Draw] test: shader {shaderPath}: {shaderManifest.Error ?? string.Join(" ", shaderManifest.Tags)}");
            }
            _actor = Actor.Create("MeshRenderer Test");
            _actor.AddTransform();
            MeshRenderer renderer = _actor.AddComponent<MeshRenderer>();
            renderer.Model = AssetReference.FromText(assets.GuidOf(parts[0]));
            renderer.Material = AssetReference.FromText(assets.GuidOf(parts[1]));
            uint modelType = Resources.FindType(Resources.ModelType);
            if (modelType != 0 && !renderer.Model.IsNone)
                _model = Resources.Acquire(modelType, new ResourceID(renderer.Model.Guid));
            Debug.Log($"[Draw] test: created {_actor.Name} with model {renderer.Model} and material {renderer.Material}");
        }

        public static void Update()
        {
            if (_actor is null)
                return;
            ++_frames;
            if (!_placed && _model.State == ResourceState.Ready && Resources.GetModel(_model, out ModelInfo info))
            {
                _placed = true;
                float extent = Math.Max(info.BoundsMax[1] - info.BoundsMin[1], Math.Max(info.BoundsMax[0] - info.BoundsMin[0], info.BoundsMax[2] - info.BoundsMin[2]));
                float scale = extent > 0.0f ? 2.0f / extent : 1.0f;
                Transform transform = _actor.Transform!;
                transform.LocalScale = new Vector3(scale, scale, scale);
                transform.LocalPosition = new Vector3(2.0f - (info.BoundsMin[0] + info.BoundsMax[0]) * 0.5f * scale, -info.BoundsMin[1] * scale,
                    -(info.BoundsMin[2] + info.BoundsMax[2]) * 0.5f * scale);
                _model.Dispose();
            }
            if (_frames == 60 || _frames == 300)
            {
                int draws = MeshRenderer.DrawCount(out int materialDraws);
                Debug.Log($"[Draw] test: frame {_frames}: {draws} draw(s), {materialDraws} with a resolved material");
            }
            if (_frames == 120 && _edit is not null && _materialPath is not null && xgEditor.CoreCLR.ProjectService.Assets is { } assets)
            {
                string[] pair = _edit.Split('=');
                xg.editor.MaterialAsset material = xg.editor.MaterialAsset.Read(assets.FullPath(_materialPath));
                float[] value = pair[1].Split(',').Select(v => float.Parse(v, System.Globalization.CultureInfo.InvariantCulture)).Concat(new float[4]).Take(4).ToArray();
                material.Params[pair[0]] = value;
                material.Write(assets.FullPath(_materialPath));
                assets.RequestBuild(_materialPath);
                Debug.Log($"[Draw] test: edited {_materialPath}: {pair[0]} = {pair[1]}");
            }
        }
    }

    // Test hook (XG_RESOURCE_TEST_OBJECT=<model path>[:<material path>]): creates <model>.xco next to the model when it's
    // missing (one Mesh Object per mesh, the material on every part, plus a "Muzzle" proxy one unit above the root),
    // builds it, creates "ObjectDriver Test" (Transform + ObjectDriverComponent) at the origin, two units tall, and logs
    // [Objects] lines: the build, the draw counts and the Muzzle proxy's world position.
    internal static unsafe class ObjectSmoke
    {
        static Actor? _actor;
        static ObjectDriverComponent? _driver;
        static ResourceHandle _model;
        static bool _placed;
        static int _frames;
        static string? _actorName;

        public static void Init()
        {
            string? spec = Environment.GetEnvironmentVariable("XG_RESOURCE_TEST_OBJECT");
            if (string.IsNullOrEmpty(spec) || ProjectService.Assets is not { } assets)
                return;
            string[] parts = spec.Split(':');
            string modelPath = parts[0];
            assets.BuildAll();
            string objectPath = modelPath[..^Path.GetExtension(modelPath).Length] + xg.editor.ObjectAsset.Extension;
            if (assets.GuidOf(objectPath) is null)
            {
                if (AssetTemplates.CreateObjectFromModel(modelPath) is not string created)
                {
                    Debug.LogError($"[Objects] test: could not create an object from {modelPath}");
                    return;
                }
                objectPath = created;
                xg.editor.ObjectAsset asset = xg.editor.ObjectAsset.Read(assets.FullPath(objectPath));
                string? material = parts.Length > 1 && parts[1] != "actors" ? assets.GuidOf(parts[1]) : null;
                foreach (xg.editor.ObjectNodeAsset node in asset.Objects)
                    foreach (xg.editor.ObjectPartAsset part in node.Parts)
                        part.Material = material ?? string.Empty;
                xg.editor.ObjectNodeAsset root = asset.Objects[0];
                asset.Objects.Add(new xg.editor.ObjectNodeAsset
                {
                    Id = xg.editor.AssetMeta.NewGuid(), Name = "Muzzle", Kind = xg.editor.ObjectNodeKind.Proxy,
                    Expose = xg.editor.ObjectNodeExpose.Proxy, Parent = root.Id, Position = new[] { 0.0f, 1.0f, 0.0f },
                });
                asset.Write(assets.FullPath(objectPath));
                assets.Refresh();
                Debug.Log($"[Objects] test: created {objectPath} with {asset.Objects.Count} Objects");
            }
            if (parts.Contains("actors"))
            {
                xg.editor.ObjectAsset asset = xg.editor.ObjectAsset.Read(assets.FullPath(objectPath));
                xg.editor.ObjectNodeAsset? body = asset.Objects.FirstOrDefault(o => o.Kind == xg.editor.ObjectNodeKind.Mesh);
                if (body is not null)
                {
                    body.Expose = xg.editor.ObjectNodeExpose.Actor;
                    body.Components = new System.Text.Json.Nodes.JsonArray(new System.Text.Json.Nodes.JsonObject
                    {
                        ["type"] = "xgEditor.CoreCLR.WeaponSample, xgEditor.CoreCLR",
                        ["fields"] = new System.Text.Json.Nodes.JsonObject { ["Ammo"] = new System.Text.Json.Nodes.JsonObject { ["type"] = "System.Int32", ["value"] = 7 } },
                    });
                    asset.Write(assets.FullPath(objectPath));
                    _actorName = body.Name;
                    Debug.Log($"[Objects] test: exposed {body.Name} as an actor with an authored WeaponSample (Ammo 7)");
                }
            }
            assets.RequestBuild(objectPath);
            assets.BuildAll();
            string? guid = assets.GuidOf(objectPath);
            xg.editor.AssetBuildManifest? manifest = guid is null ? null : assets.Pipeline.ManifestOf(guid);
            Debug.Log($"[Objects] test: {objectPath}: {(guid is null ? "no asset" : manifest?.Error ?? string.Join(" ", manifest?.Tags ?? new List<string>()))}");
            _actor = Actor.Create("ObjectDriver Test");
            _actor.AddTransform();
            _driver = _actor.AddComponent<ObjectDriverComponent>();
            _driver.Object = AssetReference.FromText(guid);
            uint modelType = Resources.FindType(Resources.ModelType);
            string? modelGuid = assets.GuidOf(modelPath);
            if (modelType != 0 && ResourceID.TryParse(modelGuid, out ResourceID modelId))
                _model = Resources.Acquire(modelType, modelId);
            Debug.Log($"[Objects] test: created {_actor.Name} driving {_driver.Object}");
        }

        public static void Update()
        {
            if (_actor is null || _driver is null)
                return;
            ++_frames;
            if (!_placed && _model.State == ResourceState.Ready && Resources.GetModel(_model, out ModelInfo info))
            {
                _placed = true;
                float extent = Math.Max(info.BoundsMax[1] - info.BoundsMin[1], Math.Max(info.BoundsMax[0] - info.BoundsMin[0], info.BoundsMax[2] - info.BoundsMin[2]));
                float scale = extent > 0.0f ? 2.0f / extent : 1.0f;
                Transform transform = _actor.Transform!;
                transform.LocalScale = new Vector3(scale, scale, scale);
                transform.LocalPosition = new Vector3(-(info.BoundsMin[0] + info.BoundsMax[0]) * 0.5f * scale, -info.BoundsMin[1] * scale,
                    -(info.BoundsMin[2] + info.BoundsMax[2]) * 0.5f * scale);
                _model.Dispose();
            }
            if (_frames == 60 || _frames == 300)
            {
                int draws = ObjectDriverComponent.DrawCount(out int ready);
                string proxy = _driver.TryGetProxy("Muzzle", out TransformData muzzle)
                    ? $"Muzzle at ({muzzle.PositionX:0.###}, {muzzle.PositionY:0.###}, {muzzle.PositionZ:0.###})"
                    : "no Muzzle";
                Debug.Log($"[Objects] test: frame {_frames}: {draws} draw(s), {ready} ready object(s), {proxy}");
                if (_actorName is not null)
                {
                    ActorID composed = _driver.GetCompositionActor(_actorName);
                    List<object> components = new();
                    if (composed.Value != 0)
                        ActorTree.GetComponents(composed, components);
                    string weapon = components.OfType<WeaponSample>().FirstOrDefault() is WeaponSample sample ? $"WeaponSample Ammo {sample.Ammo}" : "no WeaponSample";
                    Debug.Log($"[Objects] test: Composition Actor {_actorName} = {composed.Value} (parent {ActorTree.GetParent(composed)?.Value ?? 0}, owner {ObjectDriverComponent.CompositionOwner(composed).Value}, driver {_actor.ID.Value}), {components.Count} component(s), {weapon}");
                }
            }
        }
    }

    // Test hook (XG_THUMBNAIL_TEST=<asset path>): deletes the composition's thumbnail after 30 frames so it's rendered
    // again, and logs when it's written.
    internal static class ThumbnailSmoke
    {
        static int _frames;
        static bool _hooked;

        public static void Update()
        {
            string? path = Environment.GetEnvironmentVariable("XG_THUMBNAIL_TEST");
            if (string.IsNullOrEmpty(path) || ++_frames != 30 || ProjectService.Assets is not { } assets || assets.GuidOf(path) is not string guid)
                return;
            if (CompositionThumbnails.PathFor(guid) is string thumbnail && File.Exists(thumbnail))
                File.Delete(thumbnail);
            CompositionThumbnails.Forget(guid);
            if (!_hooked)
            {
                _hooked = true;
                CompositionThumbnails.Written += (written, file) => Debug.Log($"[Debug] Thumbnail test: {file} ({new FileInfo(file).Length} bytes)");
            }
            CompositionThumbnails.Request(guid);
            Debug.Log($"[Debug] Thumbnail test: requested {path}");
        }
    }

    // Test hook (XG_RESOURCE_TEST=1): exercises the shared resource cache, then quits so shutdown cleanup is visible.
    internal static class ResourceSmoke
    {
        const string SharedId = "a0000000000000000000000000000001/c0000000000000000000000000000001";
        const string TransientId = "a0000000000000000000000000000002";
        const string FailingId = "ffffffffffffffff0000000000000001";
        const string DemoOwnedId = "d0000000000000000000000000000001";

        static bool _enabled;
        static int _step;
        static uint _type;
        static ResourceHandle _shared;
        static ResourceHandle _transient;
        static ResourceHandle _demoOwned;

        public static void Init()
        {
            _enabled = Environment.GetEnvironmentVariable("XG_RESOURCE_TEST") == "1";
            if (!_enabled)
                return;
            _type = Resources.RegisterTestType("test.shared");
            _shared = Resources.Acquire(_type, ResourceID.Parse(SharedId));
            Debug.Log($"[Resources] editor shared handle {_shared.Value:x}, state {_shared.State}");
        }

        public static void Update(long frames)
        {
            if (!_enabled)
                return;
            if (_step == 0 && frames >= 5)
            {
                _step++;
                ResourceHandle failing = Resources.Acquire(_type, ResourceID.Parse(FailingId));
                Debug.Log($"[Resources] failing resource state {failing.State}");
                failing.Dispose();
            }
            else if (_step == 1 && frames >= 10)
            {
                _step++;
                ResourceHandle first = Resources.Acquire(_type, ResourceID.Parse(TransientId));
                first.Dispose();
                _transient = Resources.Acquire(_type, ResourceID.Parse(TransientId));
                _transient.Dispose();
                Debug.Log($"[Resources] transient reacquired in one frame: same handle {first.Equals(_transient)}, state {_transient.State}");
            }
            else if (_step == 2 && frames >= 12)
            {
                _step++;
                Debug.Log($"[Resources] transient after collect: state {_transient.State}, second release {Resources.Release(_transient)}");
            }
            else if (_step == 3 && frames >= 20)
            {
                _step++;
                _demoOwned = Resources.Acquire(Resources.FindType("test.demo"), ResourceID.Parse(DemoOwnedId));
                Debug.Log($"[Resources] editor acquired demo-owned resource: state {_demoOwned.State}");
            }
            else if (_step == 4 && frames >= 40)
            {
                _step++;
                ScriptRuntime.RequestQuit();
            }
        }

        public static void Shutdown()
        {
            if (!_enabled)
                return;
            Debug.Log($"[Resources] editor shutdown: demo-owned state {_demoOwned.State}, shared state {_shared.State}");
            _shared.Dispose();
        }
    }

    public static unsafe class ScriptEntry
    {
        [UnmanagedCallersOnly]
        public static int Init(nint enginePtr, uint moduleID)
        {
            ScriptRuntime.Initialize(enginePtr, moduleID);
            EditorLog.Attach();
            Subsystem.Register<xg.editor.Tools>();
            PlayMode.Initialize();
            ProjectService.Open();

            // Example: register a managed log callback
            ScriptRuntime.AddLogCallback(OnLog);
            Console.WriteLine("wow");
            TestVector3();
            TestVector2();
            TestQuaternion();
            TestMatrix4x4();
            
            Subsystem.Register<SampleSubsystem>("managed.system").Name = "managed.system";
            Subsystem.Register(new SampleSubsystem { Name = "managed.temp" }, "managed.temp");
            Debug.Log($"[Debug] Subsystem managed.temp unregistered: {Subsystem.Unregister("managed.temp")}, registered: [{string.Join(", ", Subsystem.Names)}]");
            Subsystem.Register<SampleSubsystem>("managed.system");

            var actor = xg.Actor.Create("walaweh!");
            Console.WriteLine(actor.Name);
            actor.Name = "REALLY AWWESOMEH!!!";
            Console.WriteLine(actor.Name);

            _player = actor.AddComponent<PlayerComponent>();
            _player.Data1.Stats.Health = 100;

            _temporary = xg.Actor.Create("temporary");
            _temporaryEnemy = _temporary.AddComponent<EnemyComponent>();

            Debug.Log("[Debug] editor module initialized", actor);
            Debug.LogSuccess("[Debug] success level");
            Debug.LogWarningFormat("[Debug] {0} actor(s) registered, 100% {1}", ActorRegistry.Count, "ok");
            Debug.LogError("[Debug] sample error with caller stack");
            try
            {
                throw new InvalidOperationException("sample exception");
            }
            catch (Exception e)
            {
                Debug.LogException(e, _temporary);
            }
            Debug.Assert(ActorRegistry.Count == 2, "expected two actors");
            Debug.Log(null);

            xg.Actor body = xg.Actor.Create("Body");
            xg.Actor weapon = xg.Actor.Create("Weapon");
            xg.Actor muzzle = xg.Actor.Create("Muzzle");
            body.Parent = actor;
            weapon.Parent = actor;
            muzzle.Parent = weapon;
            weapon.AddComponent<WeaponSample>();
            List<ActorID> children = new();
            ActorTree.GetChildren(actor.ID, children);
            Debug.Log($"[Debug] Hierarchy sample: {actor.Name} children [{string.Join(", ", children.Select(ActorTree.GetName))}], Muzzle parent {muzzle.Parent?.Name}");

            EditorShell.Install();
            ScenePanel scene = xg.imui.Panel.Open<ScenePanel>();
            GamePanel game = xg.imui.Panel.Open<GamePanel>();
            if (Environment.GetEnvironmentVariable("XG_UI_TEST_FOCUS") == "scenepanel")
                scene.Focus();
            if (Environment.GetEnvironmentVariable("XG_UI_TEST_FOCUS") == "game")
                game.Focus();

            HierarchyPanel hierarchy = xg.imui.Panel.Open<HierarchyPanel>();
            if (Environment.GetEnvironmentVariable("XG_UI_TEST_FOCUS") == "hierarchy")
            {
                hierarchy.Focus();
                hierarchy.ExpandAll();
                hierarchy.SelectByName("Weapon");
            }

            InspectorPanel inspector = xg.imui.Panel.Open<InspectorPanel>();
            string? inspectTarget = Environment.GetEnvironmentVariable("XG_UI_TEST_INSPECT");
            if (int.TryParse(Environment.GetEnvironmentVariable("XG_UI_TEST_INSPECT_TAB"), out int inspectTab))
                inspector.SelectedTab = inspectTab;
            if (Environment.GetEnvironmentVariable("XG_UI_TEST_INSPECT_ADD") == "1")
                inspector.OpenAddComponent(30);
            string? inspectAddType = Environment.GetEnvironmentVariable("XG_UI_TEST_INSPECT_ADDTYPE");
            if (!string.IsNullOrEmpty(inspectAddType))
                inspector.AddComponentWhenSelected(inspectAddType);
            if (Environment.GetEnvironmentVariable("XG_UI_TEST_FOCUS") == "inspector")
            {
                inspector.Focus();
                hierarchy.ExpandAll();
                hierarchy.SelectByName(string.IsNullOrEmpty(inspectTarget) ? "Weapon" : inspectTarget);
            }

            ConsolePanel console = xg.imui.Panel.Open<ConsolePanel>();
            if (Environment.GetEnvironmentVariable("XG_UI_TEST_FOCUS") == "console")
            {
                console.Focus();
                console.SelectLatestError();
            }
            if (Environment.GetEnvironmentVariable("XG_UI_TEST_CONSOLE_COLLAPSE") == "1")
                console.SetCollapse(true);
            foreach (string level in (Environment.GetEnvironmentVariable("XG_UI_TEST_CONSOLE_HIDE") ?? string.Empty).Split(',', StringSplitOptions.RemoveEmptyEntries))
                if (Enum.TryParse(level, true, out MessageType hidden))
                    console.SetLevelVisible(hidden, false);
            if (Environment.GetEnvironmentVariable("XG_UI_TEST_CONSOLE_SOURCE") == "open")
                console.OpenSourceFilter();

            NavigatorPanel navigator = xg.imui.Panel.Open<NavigatorPanel>();
            string? navFolder = Environment.GetEnvironmentVariable("XG_UI_TEST_NAV_FOLDER");
            if (!string.IsNullOrEmpty(navFolder))
                navigator.SetFolder(navFolder);
            string? navMode = Environment.GetEnvironmentVariable("XG_UI_TEST_NAV_MODE");
            if (!string.IsNullOrEmpty(navMode))
                navigator.SetMode(navMode);
            string? navTags = Environment.GetEnvironmentVariable("XG_UI_TEST_NAV_TAGS");
            if (!string.IsNullOrEmpty(navTags))
                navigator.SetTags(navTags);
            string? navType = Environment.GetEnvironmentVariable("XG_UI_TEST_NAV_TYPE");
            if (!string.IsNullOrEmpty(navType))
                navigator.TypeTags(navType, 30);
            if (Environment.GetEnvironmentVariable("XG_UI_TEST_NAV_BOOKMARKS") == "1")
                navigator.OpenBookmarks(30);
            string? navSort = Environment.GetEnvironmentVariable("XG_UI_TEST_NAV_SORT");
            if (!string.IsNullOrEmpty(navSort))
                navigator.SetSort(navSort);
            string? navOptions = Environment.GetEnvironmentVariable("XG_UI_TEST_NAV_OPTIONS");
            if (!string.IsNullOrEmpty(navOptions))
                navigator.SetOptions(navOptions);
            string? navKinds = Environment.GetEnvironmentVariable("XG_UI_TEST_NAV_KINDS");
            if (!string.IsNullOrEmpty(navKinds))
                navigator.SetKinds(navKinds);
            string? navView = Environment.GetEnvironmentVariable("XG_UI_TEST_NAV_VIEW");
            if (!string.IsNullOrEmpty(navView))
                navigator.SetView(navView);
            string? navDetailSort = Environment.GetEnvironmentVariable("XG_UI_TEST_NAV_DETAILSORT");
            if (!string.IsNullOrEmpty(navDetailSort))
                navigator.SetDetailSort(navDetailSort);
            string? navRename = Environment.GetEnvironmentVariable("XG_UI_TEST_NAV_RENAME");
            int renameColon = navRename?.LastIndexOf(':') ?? -1;
            if (navRename is not null && renameColon > 0)
            {
                string renameFrom = navRename[..renameColon];
                string renameTo = navRename[(renameColon + 1)..];
                if (xg.editor.AssetNames.Extension(renameTo).Length == 0)
                    renameTo += xg.editor.AssetNames.Extension(renameFrom);
                string? error = ProjectService.Assets?.Rename(renameFrom, renameTo);
                Debug.Log($"[Debug] Navigator rename {navRename}: {error ?? "ok"}");
            }
            string? testItems = Environment.GetEnvironmentVariable("XG_ASSET_TEST_ITEMS");
            if (!string.IsNullOrEmpty(testItems) && ProjectService.Assets is { } testAssets)
            {
                xg.editor.ImportedAssetItem[] imported = { new("mesh", "Root/Body"), new("mesh", "Root/Body"), new("hierarchy", "Root") };
                var first = testAssets.UpdateItems(testItems, imported);
                var second = testAssets.UpdateItems(testItems, imported);
                var renamed = testAssets.UpdateItems(testItems, new xg.editor.ImportedAssetItem[] { new("mesh", "Root/Torso"), new("hierarchy", "Root") });
                var restored = testAssets.UpdateItems(testItems, imported);
                Debug.Log($"[Assets] items of {testItems}: {first?.Assigned.Count} assigned, reimport keeps GUIDs {first?.Assigned.SequenceEqual(second!.Assigned)}, " +
                    $"rename adds {renamed?.Items.Count - second?.Items.Count}, restore keeps GUIDs {first?.Assigned.SequenceEqual(restored!.Assigned)}");
            }
            if (Environment.GetEnvironmentVariable("XG_ASSET_TEST_IMPORT") == "1" && ProjectService.Assets is { } importAssets)
            {
                importAssets.BuildAll();
                int built = 0, valid = 0;
                foreach (AssetEntry entry in importAssets.Index.Entries.Where(e => !e.IsFolder && importAssets.Pipeline.CanImport(e.RelPath)))
                {
                    string guid = importAssets.GuidOf(entry.RelPath)!;
                    if (importAssets.Pipeline.FindFile(guid, xg.editor.TextureAssets.ProcessedExtension) is not string file)
                        continue;
                    built++;
                    byte[] bytes = File.ReadAllBytes(file);
                    xg.editor.AssetMeta meta = xg.editor.AssetMeta.Read(importAssets.FullPath(xg.editor.AssetMeta.PathFor(entry.RelPath)));
                    ResourceID id = ResourceID.Parse($"{guid}/{meta.Items[0].Guid}");
                    if (Resources.ValidateChunkFile(bytes) == ChunkFileStatus.Valid && Resources.FindChunk(bytes, id.Chunk, xg.editor.TextureAssets.PixelsChunk, out _, out ulong size) && size > 0)
                        valid++;
                }
                Debug.Log($"[Assets] import test: {built} texture(s) built, {valid} readable by the native chunk reader");
            }
            string? navRenameOpen = Environment.GetEnvironmentVariable("XG_UI_TEST_NAV_RENAME_OPEN");
            if (!string.IsNullOrEmpty(navRenameOpen))
                RenameDialog.Open(navRenameOpen, new RenameOptions { DelayFrames = 30 });
            string? navContext = Environment.GetEnvironmentVariable("XG_UI_TEST_NAV_CONTEXT");
            if (!string.IsNullOrEmpty(navContext))
                navigator.OpenContextMenu(navContext, 30);
            if (Environment.GetEnvironmentVariable("XG_UI_TEST_FOCUS") == "navigator")
                navigator.Focus();

            ToolsPanel ribbon = xg.imui.Panel.Open<ToolsPanel>();
            if (Enum.TryParse(Environment.GetEnvironmentVariable("XG_UI_TEST_RIBBON_SIDE"), true, out xg.imui.RibbonSide ribbonSide))
                ribbon.Ribbon.Side = ribbonSide;
            if (Environment.GetEnvironmentVariable("XG_UI_TEST_FOCUS") == "ribbon")
            {
                ribbon.Focus();
                if (!string.IsNullOrEmpty(inspectTarget))
                {
                    hierarchy.ExpandAll();
                    hierarchy.SelectByName(inspectTarget);
                }
            }
            if (int.TryParse(Environment.GetEnvironmentVariable("XG_UI_TEST_RIBBON_TAB"), out int ribbonTab))
                ribbon.Ribbon.SelectedIndex = ribbonTab;
            if (Environment.GetEnvironmentVariable("XG_UI_TEST_INSPECT_LOCK") == "1")
                inspector.LockWhenShowing();
            if (Environment.GetEnvironmentVariable("XG_UI_TEST_INSPECT_EXTRA") == "1")
                Subsystem.Get<xg.editor.Tools>()?.OpenPanel<InspectorPanel>();
            string? selectAfter = Environment.GetEnvironmentVariable("XG_UI_TEST_SELECT_AFTER");
            if (!string.IsNullOrEmpty(selectAfter))
                hierarchy.SelectByNameLater(selectAfter, 60);
            string? ribbonMore = Environment.GetEnvironmentVariable("XG_UI_TEST_RIBBON_MORE");
            if (!string.IsNullOrEmpty(ribbonMore))
                ribbon.Ribbon.RequestOverflow(ribbonMore, 30);

            _testWindow = xg.imui.Panel.Open<ManagedTestWindow>();
            xg.imui.Panel.Open<ManagedFloatingWindow>();
            xg.imui.Panel.Open<ManagedSceneViewWindow>();
            if (Environment.GetEnvironmentVariable("XG_UI_TEST_FOCUS") == "managed")
                _testWindow.Focus();
            Debug.Log($"[Debug] Managed Test registered: {_testWindow.IsRegistered}, xgImUIKit API {xg.imui.ImUI.NativeApiVersion}");
            string? inspectAsset = Environment.GetEnvironmentVariable("XG_UI_TEST_INSPECT_ASSET");
            if (!string.IsNullOrEmpty(inspectAsset))
            {
                xg.editor.Selection.Set(new xg.editor.AssetRef(inspectAsset));
                inspector.Focus();
            }
            string? textureTests = Environment.GetEnvironmentVariable("XG_ASSET_TEST_TEXTURE");
            if (!string.IsNullOrEmpty(textureTests) && ProjectService.Assets is { } textureAssets)
            {
                foreach (string test in textureTests.Split(';', StringSplitOptions.RemoveEmptyEntries))
                {
                    int colon = test.IndexOf(':');
                    string path = test[..colon];
                    string guid = textureAssets.GuidOf(path) ?? string.Empty;
                    xg.editor.AssetMeta meta = xg.editor.AssetMeta.Read(textureAssets.FullPath(xg.editor.AssetMeta.PathFor(path)));
                    var process = meta.ProcessSettings;
                    xg.editor.TextureSettings.From(process).WithText(test[(colon + 1)..]).WriteTo(process);
                    textureAssets.SetProcessSettings(path, process);
                    textureAssets.BuildAll();
                    string file = textureAssets.Pipeline.FindFile(guid, xg.editor.TextureAssets.ProcessedExtension) ?? string.Empty;
                    byte[] bytes = file.Length > 0 ? File.ReadAllBytes(file) : Array.Empty<byte>();
                    meta = xg.editor.AssetMeta.Read(textureAssets.FullPath(xg.editor.AssetMeta.PathFor(path)));
                    ResourceGUID item = ResourceID.Parse($"{guid}/{meta.Items[0].Guid}").Chunk;
                    Resources.FindChunk(bytes, item, xg.editor.TextureAssets.PixelsChunk, out _, out ulong size);
                    var chunk = xg.editor.ChunkFile.Parse(bytes).Items[0].Chunks.First(c => c.Type == xg.editor.TextureAssets.PixelsChunk);
                    Debug.Log($"[Assets] texture test {test}: {(xg.editor.TextureHardwareFormat)chunk.Extra}, {size} byte(s), native {Resources.ValidateChunkFile(bytes)}");
                }
            }
            ResourceSmoke.Init();
            if (Environment.GetEnvironmentVariable("XG_JOB_TEST") == "1")
                ManagedJobTest();
            TextureSmoke.Init();
            ModelSmoke.Init();
            MaterialSmoke.Init();
            ObjectSmoke.Init();
            TransformSmoke.Init();
            DataSmoke.Init();
            return 1;
        }

        static ManagedTestWindow _testWindow = null!;

        static PlayerComponent _player = null!;
        static xg.Actor _temporary = null!;
        static EnemyComponent _temporaryEnemy = null!;
        static bool _reported;

        static readonly System.Diagnostics.Stopwatch _clock = new();
        static float _elapsedAtStart;
        static int _selectionFrames;
        static long _updateFrames;

        // XG_UI_TEST_SELECT_ADD=<asset path> adds that asset to the selection after 60 frames, and
        // XG_UI_TEST_SELECT_SET=<asset path> replaces the selection with it after 120 frames.
        // XG_JOB_TEST=1: 16 managed worker jobs (one throws on purpose), joined by a main-thread continuation.
        private static void ManagedJobTest()
        {
            int mainThread = Environment.CurrentManagedThreadId;
            int ran = 0;
            HashSet<int> threads = new();
            List<JobHandle> work = new();
            for (int i = 0; i < 16; i++)
            {
                int index = i;
                work.Add(Jobs.Schedule(() =>
                {
                    Thread.Sleep(4);
                    lock (threads)
                        threads.Add(Environment.CurrentManagedThreadId);
                    Interlocked.Increment(ref ran);
                    if (index == 7)
                        throw new InvalidOperationException("expected test exception from a managed job");
                }));
            }
            Jobs.Schedule(() => Debug.Log($"[Jobs] managed: {Volatile.Read(ref ran)}/16 ran on {threads.Count} worker thread(s) (main thread among them: {threads.Contains(mainThread)}), " +
                $"continuation on the main thread: {(Environment.CurrentManagedThreadId == mainThread ? "yes" : "no")}, {Jobs.WorkerCount} worker(s)"), JobQueue.Main, work.ToArray());
        }

        private static void UpdateSelectionHooks()
        {
            int frame = ++_selectionFrames;
            if (frame == 20 && Environment.GetEnvironmentVariable("XG_UI_TEST_SCRIPT_BUILD") == "1")
                _ = ProjectService.BuildScripts();
            if (frame == 20 && Environment.GetEnvironmentVariable("XG_UI_TEST_MATERIAL_EDIT") is { Length: > 0 } materialEdit)
            {
                string[] parts = materialEdit.Split(':', 2);
                MaterialEditorPanel panel = MaterialEditorPanel.Open(parts[0]);
                if (parts.Length > 1 && panel.Driver is { } driver && parts[1].Split('=') is { Length: 2 } pair)
                {
                    float[] value = pair[1].Split(',').Select(v => float.Parse(v, System.Globalization.CultureInfo.InvariantCulture)).Concat(new float[4]).Take(4).ToArray();
                    driver.SetParam(pair[0], value);
                    Debug.Log($"[Debug] Material draft {parts[0]}: {pair[0]} = {pair[1]} (pending: {driver.HasPendingChanges})");
                }
                if (Environment.GetEnvironmentVariable("XG_UI_TEST_MATERIAL_PREVIEW_MENU") == "1")
                    panel.View.Preview.OpenMenu(30);
            }
            if (frame == 20 && Environment.GetEnvironmentVariable("XG_UI_TEST_OBJECT_EDIT") is { Length: > 0 } objectEdit)
            {
                string[] parts = objectEdit.Split(':');
                ObjectEditorPanel panel = ObjectEditorPanel.Open(parts[0]);
                foreach (string option in parts.Skip(1))
                {
                    string[] pair = option.Split('=', 2);
                    if (pair[0] == "select" && pair.Length > 1)
                        Debug.Log($"[Debug] Object editor select {pair[1]}: {panel.View.SelectByName(pair[1])}");
                    else if (pair[0] == "tab" && pair.Length > 1 && int.TryParse(pair[1], out int tab))
                        panel.View.Tab = tab;
                    else if (pair[0] == "menu")
                        panel.View.Preview.OpenMenu(30);
                    else if (pair[0] == "editmaterial" && panel.Driver is { } owner && panel.View.Selected is string selectedId
                        && owner.Find(selectedId)?.Parts.FirstOrDefault(p => p.Material.Length > 0) is { } part)
                        panel.View.EditMaterial = part.Material;
                    else if (pair[0] == "addcomponent" && pair.Length > 1 && panel.Driver is { } author && panel.View.Selected is string authorId
                        && author.Find(authorId) is { } authorNode && xg.editor.ComponentCatalog.Types.FirstOrDefault(t => t.Type.Name == pair[1]) is { } componentType)
                    {
                        authorNode.Expose = xg.editor.ObjectNodeExpose.Actor;
                        panel.View.AddAuthored(author, authorNode, componentType);
                        Debug.Log($"[Debug] Object authored {componentType.Key} on {authorNode.Name}");
                    }
                    else if (pair[0] == "save" && panel.Driver is { } saved)
                    {
                        saved.Apply();
                        Debug.Log($"[Debug] Object saved {parts[0]} (pending: {saved.HasPendingChanges})");
                    }
                    else if (pair[0] == "edit" && pair.Length > 1 && panel.Driver is { } driver && panel.View.Selected is string id && driver.Find(id) is { } node)
                    {
                        node.Position = pair[1].Split(',').Select(v => float.Parse(v, System.Globalization.CultureInfo.InvariantCulture)).Concat(new float[3]).Take(3).ToArray();
                        driver.Changed();
                        Debug.Log($"[Debug] Object draft {parts[0]}: {node.Name} position = {pair[1]} (pending: {driver.HasPendingChanges})");
                    }
                }
            }
            if (frame == 30 && Environment.GetEnvironmentVariable("XG_UI_TEST_TASKS_CANCEL") is { Length: > 0 } cancelName)
            {
                BackgroundTask? task = BackgroundTasks.Find(cancelName);
                Debug.Log($"[Debug] Cancel task {cancelName}: {(task is null ? "not found" : task.Title + ", " + task.State)}");
                task?.Cancel();
            }
            if (frame == 60 && Environment.GetEnvironmentVariable("XG_UI_TEST_SELECT_ADD") is { Length: > 0 } add)
            {
                xg.editor.Selection.Add(new xg.editor.AssetRef(add));
                Debug.Log($"[Debug] Selection add {add}: {xg.editor.Selection.Count} item(s)");
            }
            if (frame == 120 && Environment.GetEnvironmentVariable("XG_UI_TEST_SELECT_SET") is { Length: > 0 } set)
            {
                xg.editor.Selection.Set(new xg.editor.AssetRef(set));
                Debug.Log($"[Debug] Selection set {set}");
            }
        }

        [UnmanagedCallersOnly]
        public static void Update(float dt)
        {
            ProjectService.Assets?.Update();
            ProjectService.Update();
            AssetThumbnails.NewFrame();
            PlayMode.Update(dt);
            UpdateSelectionHooks();
            long frames = ++_updateFrames;
            ResourceSmoke.Update(frames);
            TextureSmoke.Update();
            ModelSmoke.Update();
            MaterialSmoke.Update();
            ObjectSmoke.Update();
            ThumbnailSmoke.Update();
            CompositionThumbnails.Update();
            CompositionComponents.Update();
            TransformSmoke.Update();
            DataSmoke.Update();
            if (frames == 1 && !_clock.IsRunning)
            {
                _elapsedAtStart = _player.ElapsedTime;
                _clock.Start();
            }
            if (_temporary.IsValid && frames >= 30)
            {
                _temporary.Destroy();
                Debug.Log($"[Actors] destroyed 'temporary' after {frames} frames (EnemyComponent updated {_temporaryEnemy.UpdateCount} times)");
            }
            if (!_reported && frames >= 61)
            {
                _reported = true;
                double wall = _clock.Elapsed.TotalSeconds;
                double summed = _player.ElapsedTime - _elapsedAtStart;
                Debug.Log($"[Actors] CoreCLR: {ActorRegistry.Count} actor(s) after {frames} frames: " +
                    $"PlayerComponent updated {_player.UpdateCount} times, destroyed actor's EnemyComponent {_temporaryEnemy.UpdateCount} times");
                if (_player.UpdateCount > 0)
                    Debug.Log($"[Actors] delta time over 60 frames: summed dt {summed:F4} s, wall clock {wall:F4} s, " +
                        $"difference {(summed - wall) * 1000.0:F2} ms, average frame {wall / 60.0 * 1000.0:F2} ms");
            }
        }

        [UnmanagedCallersOnly]
        public static void Shutdown()
        {
            ResourceSmoke.Shutdown();
            xg.imui.ImUI.ShutdownAll();
            ProjectService.Assets?.Flush();
            ProjectService.Shutdown();
            EditorLog.Detach();
            ScriptRuntime.Shutdown();
        }

        private static void OnLog(MessageType type, IntPtr msgPtr)
        {
            string msg = Marshal.PtrToStringAnsi(msgPtr) ?? string.Empty;
           // Console.WriteLine($"[{type}] {msg} waasdadsds");
        }

        public class TestType
        {
            public int Health { get; set; }
            public string Name { get; set; } = string.Empty;
        }

        [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
        public static void OnMessage(nint typeName, nint payload, int payloadSize)
        {
            string typeNameStr = Marshal.PtrToStringAnsi(typeName) ?? string.Empty;

            if (payload == 0 || payloadSize == 0)
                return;

            if (ScriptRuntime.PayloadMode == PayloadMode.BINARY)
            {
                // Build interop struct
                var msg = new ScriptMessageInterop
                {
                    Payload = (byte*)payload,
                    PayloadSize = payloadSize,
                    TypeName = typeNameStr,
                    SourceID = 0 // managed
                };

                var parser = new ScriptMessageParser(msg);

                // Example decode for TestType
                if (typeNameStr == "TestType")
                {
                    int health = parser.ReadPrimitive<int>();
                    string name = parser.ReadString();

                    var obj = new TestType { Health = health, Name = name };

                    ScriptRuntime.DispatchMessage(typeNameStr, obj);
                    return;
                }

                Console.WriteLine($"[Managed] Unknown binary type: {typeNameStr}");
                return;
            }
            else
            {
                // JSON path (your existing logic)
                byte[] data = new byte[payloadSize];
                Marshal.Copy(payload, data, 0, payloadSize);

                string json = Encoding.UTF8.GetString(data);
                Console.WriteLine($"[Managed] Raw JSON: {json}");

                if (typeNameStr == "TestType")
                {
                    var obj2 = JsonSerializer.Deserialize<TestType>(json);
                    if (obj2 is not null)
                        ScriptRuntime.DispatchMessage(typeNameStr, obj2);
                }
                else
                {
                    Console.WriteLine($"Unknown type {typeNameStr}: {json}");
                }
            }
        }

        public static void TestVector3()
        {
            Console.WriteLine("=== XGE Vec3 Interop Test ===");

            Vector3 a = new Vector3(1, 2, 3);
            Vector3 b = new Vector3(4, 5, 6);
            Vector3 min = new Vector3(0, 0, 0);
            Vector3 max = new Vector3(10, 10, 10);

            Matrix4x4 identity = new Matrix4x4
            {
                m00 = 1,
                m01 = 0,
                m02 = 0,
                m03 = 0,
                m10 = 0,
                m11 = 1,
                m12 = 0,
                m13 = 0,
                m20 = 0,
                m21 = 0,
                m22 = 1,
                m23 = 0,
                m30 = 0,
                m31 = 0,
                m32 = 0,
                m33 = 1
            };

            // Arithmetic
            Console.WriteLine($"Add: {a.Add(b).x}, {a.Add(b).y}, {a.Add(b).z}");
            Console.WriteLine($"Sub: {a.Sub(b).x}, {a.Sub(b).y}, {a.Sub(b).z}");
            Console.WriteLine($"Mul: {a.Mul(2).x}, {a.Mul(2).y}, {a.Mul(2).z}");
            Console.WriteLine($"Div: {a.Div(2).x}, {a.Div(2).y}, {a.Div(2).z}");

            // Vector math
            Console.WriteLine($"Length(a): {a.Length()}");
            Console.WriteLine($"LengthSquared(a): {a.LengthSquared()}");
            Vector3 norm = a.Normalized();
            Console.WriteLine($"Normalize(a): {norm.x}, {norm.y}, {norm.z}");

            Console.WriteLine($"Dot(a,b): {a.Dot(b)}");
            Vector3 cross = a.Cross(b);
            Console.WriteLine($"Cross(a,b): {cross.x}, {cross.y}, {cross.z}");

            // Distance
            Console.WriteLine($"Distance(a,b): {a.Distance(b)}");
            Console.WriteLine($"DistanceSquared(a,b): {a.DistanceSquared(b)}");

            // Lerp / Min / Max / Clamp
            Vector3 lerp = a.Lerp(b, 0.5f);
            Console.WriteLine($"Lerp(a,b,0.5): {lerp.x}, {lerp.y}, {lerp.z}");

            Vector3 minv = a.Min(b);
            Console.WriteLine($"Min(a,b): {minv.x}, {minv.y}, {minv.z}");

            Vector3 maxv = a.Max(b);
            Console.WriteLine($"Max(a,b): {maxv.x}, {maxv.y}, {maxv.z}");

            Vector3 clamped = a.Clamp(min, max);
            Console.WriteLine($"Clamp(a,0..10): {clamped.x}, {clamped.y}, {clamped.z}");

            // Transform
            Vector3 transformed = a.Transform(identity);
            Console.WriteLine($"Transform(a, identity): {transformed.x}, {transformed.y}, {transformed.z}");

            Vector3 dirTransformed = a.TransformDirection(identity);
            Console.WriteLine($"TransformDirection(a, identity): {dirTransformed.x}, {dirTransformed.y}, {dirTransformed.z}");

            // Utility
            Console.WriteLine($"IsZero(a): {a.IsZero()}");
            Console.WriteLine($"IsEqual(a,b): {a.IsEqual(b)}");
            Console.WriteLine($"IsEqual(a,a): {a.IsEqual(a)}");

            Console.WriteLine("=== XGE Vec3 Interop Test Complete ===");
        }

        public static void TestVector2()
        {
            Console.WriteLine("=== XGE Vec2 Interop Test ===");

            Vector2 a = new Vector2(3f, 4f);
            Vector2 b = new Vector2(1f, 2f);
            Vector2 min = new Vector2(0f, 0f);
            Vector2 max = new Vector2(10f, 10f);

            // Arithmetic
            Vector2 add = a.Add(b);
            Console.WriteLine($"Add(a,b): {add.x}, {add.y}");

            Vector2 sub = a.Sub(b);
            Console.WriteLine($"Sub(a,b): {sub.x}, {sub.y}");

            Vector2 mul = a.Mul(2f);
            Console.WriteLine($"Mul(a,2): {mul.x}, {mul.y}");

            Vector2 div = a.Div(2f);
            Console.WriteLine($"Div(a,2): {div.x}, {div.y}");

            // Vector math
            Console.WriteLine($"Length(a): {a.Length()}");
            Console.WriteLine($"LengthSquared(a): {a.LengthSquared()}");

            Vector2 norm = a.Normalized();
            Console.WriteLine($"Normalize(a): {norm.x}, {norm.y}");

            Console.WriteLine($"Dot(a,b): {a.Dot(b)}");

            // Distance
            Console.WriteLine($"Distance(a,b): {a.Distance(b)}");
            Console.WriteLine($"DistanceSquared(a,b): {a.DistanceSquared(b)}");

            // Lerp / Min / Max / Clamp
            Vector2 lerp = a.Lerp(b, 0.5f);
            Console.WriteLine($"Lerp(a,b,0.5): {lerp.x}, {lerp.y}");

            Vector2 minv = a.Min(b);
            Console.WriteLine($"Min(a,b): {minv.x}, {minv.y}");

            Vector2 maxv = a.Max(b);
            Console.WriteLine($"Max(a,b): {maxv.x}, {maxv.y}");

            Vector2 clamped = a.Clamp(min, max);
            Console.WriteLine($"Clamp(a,0..10): {clamped.x}, {clamped.y}");

            // Utility
            Console.WriteLine($"IsZero(a): {a.IsZero()}");
            Console.WriteLine($"IsEqual(a,b): {a.IsEqual(b)}");
            Console.WriteLine($"IsEqual(a,a): {a.IsEqual(a)}");

            Console.WriteLine("=== XGE Vec2 Interop Test Complete ===");
        }

        public static void TestQuaternion()
        {
            Console.WriteLine("=== XGE Quaternion Interop Test ===");

            Quaternion qA = new Quaternion(0, 0.7071067f, 0, 0.7071067f); // 90° around Y
            Quaternion qB = new Quaternion(0.7071067f, 0, 0, 0.7071067f); // 90° around X

            // Arithmetic
            Quaternion add = qA.Add(qB);
            Console.WriteLine($"Add: {add.x}, {add.y}, {add.z}, {add.w}");

            Quaternion sub = qA.Sub(qB);
            Console.WriteLine($"Sub: {sub.x}, {sub.y}, {sub.z}, {sub.w}");

            Quaternion mulScalar = qA.Mul(2.0f);
            Console.WriteLine($"MulScalar: {mulScalar.x}, {mulScalar.y}, {mulScalar.z}, {mulScalar.w}");

            // Quaternion math
            Console.WriteLine($"Length(qA): {qA.Length()}");
            Console.WriteLine($"LengthSquared(qA): {qA.LengthSquared()}");

            Quaternion norm = qA.Normalized();
            Console.WriteLine($"Normalize(qA): {norm.x}, {norm.y}, {norm.z}, {norm.w}");

            Console.WriteLine($"Dot(qA,qB): {qA.Dot(qB)}");

            Quaternion conj = qA.Conjugate();
            Console.WriteLine($"Conjugate(qA): {conj.x}, {conj.y}, {conj.z}, {conj.w}");

            Quaternion inv = qA.Inverse();
            Console.WriteLine($"Inverse(qA): {inv.x}, {inv.y}, {inv.z}, {inv.w}");

            Quaternion mul = qA.Mul(qB);
            Console.WriteLine($"MulQuat(qA,qB): {mul.x}, {mul.y}, {mul.z}, {mul.w}");

            // Slerp
            Quaternion slerp = qA.Slerp(qB, 0.5f);
            Console.WriteLine($"Slerp(qA,qB,0.5): {slerp.x}, {slerp.y}, {slerp.z}, {slerp.w}");

            // Rotate vector
            Vector3 v = new Vector3(1, 0, 0);
            Vector3 vRot = qA.RotateVec3(v);
            Console.WriteLine($"RotateVec3(qA, (1,0,0)): {vRot.x}, {vRot.y}, {vRot.z}");

            // Identity check
            Console.WriteLine($"IsIdentity(qA): {qA.IsIdentity()}");

            // Equality
            Console.WriteLine($"IsEqual(qA,qB): {qA.IsEqual(qB)}");
            Console.WriteLine($"IsEqual(qA,qA): {qA.IsEqual(qA)}");

            Console.WriteLine("=== XGE Quaternion Interop Test Complete ===");
        }

        public static void TestMatrix4x4()
        {
            Console.WriteLine("=== XGE Matrix4x4 Interop Test ===");

            Matrix4x4 I = Matrix4x4.Identity();
            Console.WriteLine("Identity:");
            Console.WriteLine(I);

            Matrix4x4 T = Matrix4x4.Translation(3, 4, 5);
            Console.WriteLine("Translation(3,4,5):");
            Console.WriteLine(T);

            Matrix4x4 S = Matrix4x4.Scaling(2, 2, 2);
            Console.WriteLine("Scaling(2,2,2):");
            Console.WriteLine(S);

            Matrix4x4 RY = Matrix4x4.RotationY(MathF.PI * 0.5f);
            Console.WriteLine("RotationY(90°):");
            Console.WriteLine(RY);

            Matrix4x4 M = T * RY * S;
            Console.WriteLine("M = T * RY * S:");
            Console.WriteLine(M);

            Matrix4x4 MT = M.Transposed();
            Console.WriteLine("Transpose(M):");
            Console.WriteLine(MT);

            Matrix4x4 MI = M.Inverse();
            Console.WriteLine("Inverse(M):");
            Console.WriteLine(MI);

            float det = M.Determinant();
            Console.WriteLine($"Determinant(M): {det}");

            Vector3 p = new Vector3(1, 2, 3);
            Vector3 p2 = M.TransformPoint(p);
            Console.WriteLine($"TransformPoint(M, (1,2,3)): {p2}");

            Vector3 d = new Vector3(0, 1, 0);
            Vector3 d2 = M.TransformDirection(d);
            Console.WriteLine($"TransformDirection(M, (0,1,0)): {d2}");

            M.Decompose(out Vector3 scale, out Quaternion rot, out Vector3 trans);
            Console.WriteLine("Decompose(M):");
            Console.WriteLine($"  Scale: {scale}");
            Console.WriteLine($"  Rotation: {rot}");
            Console.WriteLine($"  Translation: {trans}");

            Console.WriteLine("=== XGE Matrix4x4 Interop Test Complete ===");
        }

    }
}
