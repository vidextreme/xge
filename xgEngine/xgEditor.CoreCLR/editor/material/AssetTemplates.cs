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

        // A material using the project's first shader (none when the project has no shader). Returns its path.
        public static string? CreateMaterial(string folder)
        {
            if (ProjectService.Assets is not { } assets)
                return null;
            MaterialAsset material = new() { Meta = new System.Text.Json.Nodes.JsonObject { ["guid"] = AssetMeta.NewGuid(), ["tags"] = new System.Text.Json.Nodes.JsonArray() } };
            if (AssetBrowser.Current?.List("shader").FirstOrDefault(c => ShaderAssets.IsShaderSource(c.Path)) is { Guid.Length: > 0 } shader)
                material.Shader = shader.Guid;
            return assets.CreateAsset(folder, "New Material.xcm", material.ToJSON());
        }

        // A surface shader following surface.common.hlsli, with one texture and one tint. Returns its path.
        public static string? CreateShader(string folder)
        {
            if (ProjectService.Assets is not { } assets)
                return null;
            return assets.CreateAsset(folder, "surface.new.hlsl", ShaderFallback);
        }
    }
}
