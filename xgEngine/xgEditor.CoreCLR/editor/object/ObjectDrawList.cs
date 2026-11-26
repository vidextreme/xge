// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg;
using xg.editor;
using xg.math;

namespace xgEditor.CoreCLR
{
    // The draws of a composition object asset (every Mesh Object's submeshes with their materials, placed by the Jointer
    // chain) for editor render views, plus the bounds of what's drawn. Holds the model and material resources it uses
    // until disposed. Shared by the Object Editor's preview and the .xco thumbnails.
    internal sealed unsafe class ObjectDrawList : IDisposable
    {
        private readonly Dictionary<string, ResourceHandle> _models = new(StringComparer.OrdinalIgnoreCase);
        private readonly Dictionary<string, ResourceHandle> _materials = new(StringComparer.OrdinalIgnoreCase);
        private readonly List<SceneViews.MaterialDraw> _draws = new();

        public IReadOnlyList<SceneViews.MaterialDraw> Draws => _draws;
        public int TriangleCount { get; private set; }
        // True when every referenced model is loaded and every referenced material resolved (or failed).
        public bool Complete { get; private set; }
        public bool HasBounds { get; private set; }
        public Vector3 Center { get; private set; }
        public float Radius { get; private set; } = 1.0f;

        // Local-to-root matrix of a node (the chain of its Jointers).
        public static float[] WorldOf(ObjectAsset asset, ObjectNodeAsset node)
        {
            float[] world = node.LocalMatrix();
            string parent = node.Parent;
            for (int guard = 0; parent.Length > 0 && guard < 4096 && asset.Find(parent) is ObjectNodeAsset p; guard++)
            {
                world = ObjectAssets.Multiply(world, p.LocalMatrix());
                parent = p.Parent;
            }
            return world;
        }

        private ResourceHandle Acquire(Dictionary<string, ResourceHandle> cache, string typeName, string guid)
        {
            if (cache.TryGetValue(guid, out ResourceHandle handle) && !Resources.IsStale(handle))
                return handle;
            if (handle.Value != 0)
                handle.Dispose();
            uint type = Resources.FindType(typeName);
            handle = type != 0 && ResourceID.TryParse(guid, out ResourceID id) ? Resources.Acquire(type, id) : default;
            cache[guid] = handle;
            return handle;
        }

        // includeProxies: proxies widen the bounds too (the editor preview); thumbnails frame only what's drawn.
        // A renderer material to draw in place of a material GUID (the Object Editor's Auto Apply draft), or 0.
        public Func<string, ulong>? MaterialOverride { get; set; }

        public void Build(ObjectAsset asset, bool includeProxies = true)
        {
            _draws.Clear();
            TriangleCount = 0;
            Complete = true;
            bool any = false;
            Vector3 min = default, max = default;
            void Grow(float x, float y, float z)
            {
                if (!any)
                {
                    min = max = new Vector3(x, y, z);
                    any = true;
                    return;
                }
                min = new Vector3(MathF.Min(min.x, x), MathF.Min(min.y, y), MathF.Min(min.z, z));
                max = new Vector3(MathF.Max(max.x, x), MathF.Max(max.y, y), MathF.Max(max.z, z));
            }

            foreach (ObjectNodeAsset node in asset.Objects)
            {
                float[] world = WorldOf(asset, node);
                if (node.Kind == ObjectNodeKind.Proxy && includeProxies)
                    Grow(world[12], world[13], world[14]);
                if (node.Kind != ObjectNodeKind.Mesh || !AssetMeta.IsValidGuid(node.Model))
                    continue;
                ResourceHandle model = Acquire(_models, Resources.ModelType, node.Model);
                if (model.Value == 0 || model.State == ResourceState.Failed)
                    continue;
                if (model.State != ResourceState.Ready || !Resources.GetModel(model, out ModelInfo info))
                {
                    Complete = false;
                    continue;
                }
                for (int corner = 0; corner < 8; corner++)
                {
                    var p = ObjectMath.Transform(world,
                        (corner & 1) != 0 ? info.BoundsMax[0] : info.BoundsMin[0],
                        (corner & 2) != 0 ? info.BoundsMax[1] : info.BoundsMin[1],
                        (corner & 4) != 0 ? info.BoundsMax[2] : info.BoundsMin[2]);
                    Grow(p.X, p.Y, p.Z);
                }
                foreach (ObjectPartAsset part in node.Parts)
                {
                    ulong material = AssetMeta.IsValidGuid(part.Material) ? MaterialOverride?.Invoke(part.Material) ?? 0 : 0;
                    if (material == 0 && AssetMeta.IsValidGuid(part.Material))
                    {
                        ResourceHandle handle = Acquire(_materials, Resources.MaterialType, part.Material);
                        if (handle.Value != 0 && handle.State == ResourceState.Ready)
                            material = Resources.ResolveMaterial(handle);
                        if (material == 0 && handle.Value != 0 && handle.State != ResourceState.Failed)
                            Complete = false;
                    }
                    ResourceID.TryParse(part.Submesh, out ResourceID submesh);
                    for (int m = 0; m < info.PartCount; m++)
                    {
                        if (!Resources.GetModelPart(model, m, out ModelPartInfo modelPart))
                            continue;
                        if (!submesh.File.IsZero && (!Resources.GetModelPartItem(model, m, out ResourceGUID item) || !item.Equals(submesh.File)))
                            continue;
                        float[] partWorld = new float[16];
                        for (int i = 0; i < 16; i++)
                            partWorld[i] = modelPart.World[i];
                        float[] placed = ObjectAssets.Multiply(partWorld, world);
                        SceneViews.MaterialDraw entry = default;
                        entry.Mesh = modelPart.Mesh;
                        entry.Material = material;
                        for (int i = 0; i < 16; i++)
                            entry.World[i] = placed[i];
                        _draws.Add(entry);
                        TriangleCount += modelPart.TriangleCount;
                    }
                }
            }
            HasBounds = any;
            if (any)
            {
                Center = new Vector3((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f, (min.z + max.z) * 0.5f);
                float dx = max.x - min.x, dy = max.y - min.y, dz = max.z - min.z;
                Radius = MathF.Max(MathF.Sqrt(dx * dx + dy * dy + dz * dz) * 0.5f, 0.1f);
            }
        }

        // Hands the draws to a render view.
        public void Submit(ulong view)
        {
            SceneViews.MaterialDraw[] draws = _draws.ToArray();
            fixed (SceneViews.MaterialDraw* p = draws)
                SceneViews.SetMaterialDraws(view, p, (uint)draws.Length);
        }

        public void Dispose()
        {
            foreach (ResourceHandle handle in _models.Values.Concat(_materials.Values))
                if (handle.Value != 0)
                    handle.Dispose();
            _models.Clear();
            _materials.Clear();
            _draws.Clear();
        }
    }
}
