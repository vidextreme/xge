// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg.editor;

namespace xgEditor.CoreCLR
{
    // New assets made from the editor's Create menus.
    public static class AssetTemplates
    {
        private const string ShaderFallback =
            "// A lit surface: albedo texture times a tint.\r\n\r\n" +
            "#include \"surface.common.hlsli\"\r\n\r\n" +
            "//@group Surface\r\n" +
            "//@texture Albedo color\r\n" +
            "//@param Tint color 1,1,1,1\r\n\r\n" +
            "PSInput VSMain(VSInput input)\r\n{\r\n    return XGSurfaceVS(input);\r\n}\r\n\r\n" +
            "float4 PSMain(PSInput input) : SV_Target\r\n{\r\n" +
            "    float4 albedo = Texture0.Sample(Sampler0, input.uv) * input.col * Params[0];\r\n" +
            "    return float4(albedo.rgb * XGLighting(input.normal), albedo.a);\r\n}\r\n";

        // A material using the project's first shader, or the built-in lit shader when the project has none. Returns its path.
        public static string? CreateMaterial(string folder)
        {
            if (ProjectService.Assets is not { } assets)
                return null;
            MaterialAsset material = new() { Meta = new System.Text.Json.Nodes.JsonObject { ["guid"] = AssetMeta.NewGuid(), ["tags"] = new System.Text.Json.Nodes.JsonArray() } };
            if (AssetBrowser.Current?.List("shader").FirstOrDefault(c => ShaderAssets.IsShaderSource(c.Path)) is { Guid.Length: > 0 } shader)
                material.Shader = shader.Guid;
            else if (EngineAssets.LitShader.Length > 0)
                material.Shader = EngineAssets.LitShader;
            return assets.CreateAsset(folder, "New Material.xcm", material.ToJSON());
        }

        // An empty object: one root proxy named after the file. Returns its path.
        public static string? CreateObject(string folder)
        {
            if (ProjectService.Assets is not { } assets)
                return null;
            ObjectAsset asset = new() { Meta = NewMeta() };
            asset.Objects.Add(new ObjectNodeAsset { Id = AssetMeta.NewGuid(), Name = "Root", Kind = ObjectNodeKind.Proxy });
            return assets.CreateAsset(folder, "New Object.xco", asset.ToJSON());
        }

        // An object next to an imported model: a root proxy and one Mesh Object per mesh of the model, drawing its
        // submeshes (from the model's item table). Returns its path, or null when the model has no submesh items yet.
        public static string? CreateObjectFromModel(string modelPath)
        {
            if (ProjectService.Assets is not { } assets || assets.GuidOf(modelPath) is not string modelGuid)
                return null;
            if (!AssetMetaStore.TryRead(assets.FullPath(modelPath), out AssetMeta? meta) || meta is null)
                return null;
            string name = AssetNames.DisplayName(modelPath);
            ObjectAsset asset = ObjectAsset.FromModel(name, modelGuid, meta.Items);
            if (asset.Objects.Count < 2)
            {
                xg.Debug.LogWarning($"[Objects] {modelPath} has no submeshes yet (build it first)");
                return null;
            }
            asset.Meta = NewMeta();
            asset.Meta["names"] = new System.Text.Json.Nodes.JsonObject { [modelGuid] = modelPath };
            string folder = modelPath.Contains('/') ? modelPath[..modelPath.LastIndexOf('/')] : AssetIndex.RootFolder;
            return assets.CreateAsset(folder, name + ObjectAsset.Extension, asset.ToJSON());
        }

        // An empty scene. Returns its path.
        public static string? CreateScene(string folder)
        {
            if (ProjectService.Assets is not { } assets)
                return null;
            SceneAsset scene = new() { Meta = NewMeta() };
            return assets.CreateAsset(folder, "New Scene" + SceneAsset.Extension, scene.ToJSON());
        }

        private static System.Text.Json.Nodes.JsonObject NewMeta() =>
            new() { ["guid"] = AssetMeta.NewGuid(), ["tags"] = new System.Text.Json.Nodes.JsonArray() };

        // A surface shader following surface.common.hlsli, with one texture and one tint. Returns its path.
        public static string? CreateShader(string folder)
        {
            if (ProjectService.Assets is not { } assets)
                return null;
            return assets.CreateAsset(folder, "surface.new.hlsl", ShaderFallback);
        }
    }
}
