// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once
#include "xgRenderer.h"
#include "volk.h"
#include <memory>
#include <utility>
#include <vector>

namespace xg {

    class RendererVulkan : public Renderer {
    public:
        RendererVulkan() = default;
        ~RendererVulkan() override;

        bool Initialize(Window& window);

        void Render() override;
        void* GetNativeHandle() const override;

        uint64_t CreateUITexture(int32_t width, int32_t height, const void* rgba) override;
        void UpdateUITexture(uint64_t id, int32_t x, int32_t y, int32_t width, int32_t height, const void* rgba, int32_t pitch) override;
        void DestroyUITexture(uint64_t id) override;
        void SubmitUI(const UIDrawList* list) override;
        void SetClearColor(const Color& color) override;

        uint64_t CreateRenderView(int32_t width, int32_t height) override;
        bool ResizeRenderView(uint64_t id, int32_t width, int32_t height) override;
        void DestroyRenderView(uint64_t id) override;
        void SetRenderViewCamera(uint64_t id, const RenderViewCamera* camera) override;

        bool SupportsTextureFormat(TextureFormat format) const override;
        uint64_t CreateTexture(const TextureDesc* desc, const TextureMip* mips) override;
        void DestroyTexture(uint64_t id) override;
        void SetRenderViewTexture(uint64_t viewId, uint64_t textureId, RenderViewTextureMode mode) override;
        uint64_t CreateMesh(const MeshDesc* desc) override;
        void DestroyMesh(uint64_t id) override;
        void SetRenderViewDraws(uint64_t viewId, const RenderViewDraw* draws, uint32_t count) override;
        ShaderFormat GetShaderFormat() const override { return ShaderFormat::SPIRV; }
        uint64_t CreateShader(const ShaderDesc* desc) override;
        void DestroyShader(uint64_t id) override;
        uint64_t CreateMaterial(const MaterialDesc* desc) override;
        bool UpdateMaterial(uint64_t id, const MaterialDesc* desc) override;
        void DestroyMaterial(uint64_t id) override;
        void SetSceneDraws(const SceneDraw* draws, uint32_t count) override;
        void SetRenderViewMaterialDraws(uint64_t viewId, const SceneDraw* draws, uint32_t count) override;
        void SetRenderViewFlags(uint64_t viewId, RenderViewFlags flags) override;
        bool ReadRenderView(uint64_t viewId, void* rgba, uint64_t capacity) override;

        bool SupportsSurfaces() const override { return !_failed; }
        uint64_t CreateUISurface(Window& window) override;
        void DestroyUISurface(uint64_t surface) override;
        void SubmitUISurface(uint64_t surface, const UIDrawList* list) override;

    private:
        struct Buffer
        {
            VkBuffer Handle = VK_NULL_HANDLE;
            VkDeviceMemory Memory = VK_NULL_HANDLE;
            void* Mapped = nullptr;
            VkDeviceSize Size = 0;
        };

        struct Image
        {
            VkImage Handle = VK_NULL_HANDLE;
            VkDeviceMemory Memory = VK_NULL_HANDLE;
            VkImageView View = VK_NULL_HANDLE;
            // Render view color only: the sRGB attachment view (View is the UNORM view the UI samples).
            VkImageView TargetView = VK_NULL_HANDLE;
        };

        struct UITexture
        {
            Image Target;
            int32_t Width = 0;
            int32_t Height = 0;
            // Created by CreateTexture (any format, mips): not updatable with UpdateUITexture.
            bool Sampled = false;
        };

        // One swapchain-backed presentation target: the main window, or a secondary editor viewport window.
        struct Surface
        {
            uint64_t Id = 0;
            HWND Hwnd = nullptr;
            bool Immediate = false;
            VkSurfaceKHR Handle = VK_NULL_HANDLE;
            VkSwapchainKHR Swapchain = VK_NULL_HANDLE;
            VkFormat Format = VK_FORMAT_UNDEFINED;
            VkPresentModeKHR PresentMode = VK_PRESENT_MODE_FIFO_KHR;
            uint32_t Width = 0;
            uint32_t Height = 0;
            std::vector<VkImage> Images;
            std::vector<VkImageView> Views;
            std::vector<VkSemaphore> RenderFinished;
            VkSemaphore Acquired = VK_NULL_HANDLE;
            uint32_t ImageIndex = 0;
            bool Stale = false;
            Buffer VertexBuffer;
            Buffer IndexBuffer;
            size_t VertexCapacity = 0;
            size_t IndexCapacity = 0;
            std::vector<UIVertex> Vertices;
            std::vector<uint16_t> Indices;
            std::vector<UIDrawCommand> Commands;
            float Display[4] = {};
            float Scale[2] = { 1.0f, 1.0f };
            bool Skip = false;
        };

        struct RenderView
        {
            uint64_t Id = 0;
            Image Depth;
            uint32_t Width = 0;
            uint32_t Height = 0;
            RenderViewCamera Camera = {};
            uint64_t Texture = 0;
            RenderViewTextureMode Mode = RenderViewTextureMode::Color;
            std::vector<RenderViewDraw> Draws;
            std::vector<SceneDraw> MaterialDraws;
            RenderViewFlags Flags = RenderViewFlags::None;
        };

        struct GpuShader
        {
            VkPipeline Pipeline = VK_NULL_HANDLE;
        };

        // Set: XGMaterial UBO (Constants, mapped) + textures t0..t3; Views caches the image views written into it, so
        // RefreshMaterialSets rewrites the set only when a texture changed or was destroyed.
        struct GpuMaterial
        {
            uint64_t Shader = 0;
            uint64_t Textures[MaterialTextureSlots] = {};
            VkImageView Views[MaterialTextureSlots] = {};
            Buffer Constants;
            VkDescriptorSet Set = VK_NULL_HANDLE;
        };

        struct GpuMesh
        {
            Buffer Vertices;
            Buffer Indices;
            uint32_t IndexCount = 0;
            VkIndexType IndexType = VK_INDEX_TYPE_UINT16;
        };

        bool CreateInstance();
        bool PickPhysicalDevice();
        bool CreateDevice();
        bool CreateCommandObjects();
        bool CreateUIPipelineObjects();
        VkPipeline GetUIPipeline(VkFormat format);
        bool CreateSurface(Surface& surface);
        bool CreateSwapchain(Surface& surface);
        void DestroySwapchainViews(Surface& surface);
        void DestroySurface(Surface& surface);
        bool ResizeIfNeeded(Surface& surface, bool& skipFrame);
        bool DrawSurface(Surface& surface, bool withRenderViews);
        void RecordSurface(Surface& surface);
        void RecordUI(Surface& surface);
        bool EnsureUIBuffers(Surface& surface, size_t vertexCount, size_t indexCount);
        void StoreDrawList(Surface& surface, const UIDrawList* list);
        Surface* FindSurface(uint64_t id);

        bool CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, Buffer& buffer);
        void DestroyBuffer(Buffer& buffer);
        bool CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspect, Image& image,
            uint32_t mipLevels = 1, VkImageCreateFlags flags = 0);
        void DestroyImage(Image& image);
        bool FindMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties, uint32_t& index) const;
        VkCommandBuffer BeginUpload();
        bool EndUpload();
        bool UploadTextureRegion(VkImage image, int32_t x, int32_t y, int32_t width, int32_t height,
            const void* rgba, int32_t pitch, bool firstUpload);
        void WriteTextureDescriptor(uint32_t slot, VkImageView view);
        VkShaderModule CreateShaderModule(const uint32_t* code, size_t size);
        UITexture* FindTexture(uint64_t id);
        bool Check(VkResult result, const char* what);

        bool CreateScenePipeline();
        bool CreateTexturedPipeline();
        bool CreateLitPipeline();
        bool BuildMeshPipeline(VkPipelineLayout layout, VkShaderModule vertexShader, VkShaderModule pixelShader, VkPipeline& pipeline, const char* what);
        bool CreateSurfaceLayout();
        GpuMesh* FindMesh(uint64_t id);
        GpuShader* FindShader(uint64_t id);
        GpuMaterial* FindMaterial(uint64_t id);
        void WriteMaterial(GpuMaterial& material, const MaterialDesc& desc);
        void RefreshMaterialSets();
        void RecordLitMesh(const GpuMesh& mesh, VkPipelineLayout layout, const float* world, const float* viewProjection);
        void RecordSceneDraws(const std::vector<SceneDraw>& draws, const float* viewProjection, bool skipEditorHidden);
        bool CreateRenderViewTargets(RenderView& view, uint32_t width, uint32_t height);
        void RecordRenderViews();
        RenderView* FindRenderView(uint64_t id);

        static constexpr uint32_t TextureSlots = 256;
        static constexpr uint32_t MaxRenderViews = 16;
        static constexpr VkFormat UITextureFormat = VK_FORMAT_R8G8B8A8_UNORM;
        // Render views write linear color through an sRGB view of their UNORM color image.
        static constexpr VkFormat ViewColorTarget = VK_FORMAT_R8G8B8A8_SRGB;
        static constexpr VkFormat DepthFormat = VK_FORMAT_D32_SFLOAT;

        Surface _main;
        std::vector<std::unique_ptr<Surface>> _surfaces;
        uint64_t _nextSurfaceId = 1;

        VkInstance _instance = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT _debugMessenger = VK_NULL_HANDLE;
        VkPhysicalDevice _physicalDevice = VK_NULL_HANDLE;
        VkPhysicalDeviceMemoryProperties _memoryProperties = {};
        uint32_t _queueFamily = 0;
        VkDevice _device = VK_NULL_HANDLE;
        VkQueue _queue = VK_NULL_HANDLE;
        VkCommandPool _commandPool = VK_NULL_HANDLE;
        VkCommandBuffer _frameCommands = VK_NULL_HANDLE;
        VkCommandBuffer _uploadCommands = VK_NULL_HANDLE;
        VkFence _fence = VK_NULL_HANDLE;

        float _clearColor[4] = { 0.1f, 0.2f, 0.4f, 1.0f };
        bool _failed = false;
        bool _supportsBC = false;

        VkSampler _sampler = VK_NULL_HANDLE;
        VkDescriptorSetLayout _uiSetLayout = VK_NULL_HANDLE;
        VkPipelineLayout _uiPipelineLayout = VK_NULL_HANDLE;
        VkShaderModule _uiVertexShader = VK_NULL_HANDLE;
        VkShaderModule _uiPixelShader = VK_NULL_HANDLE;
        std::vector<std::pair<VkFormat, VkPipeline>> _uiPipelines;
        VkDescriptorPool _descriptorPool = VK_NULL_HANDLE;
        std::vector<VkDescriptorSet> _textureSets;
        std::vector<UITexture> _textures;
        std::vector<uint32_t> _freeTextureSlots;

        std::vector<RenderView> _renderViews;
        VkPipelineLayout _scenePipelineLayout = VK_NULL_HANDLE;
        VkPipeline _sceneLinePipeline = VK_NULL_HANDLE;
        Buffer _sceneVertexBuffer;
        uint32_t _sceneLineVertexCount = 0;
        VkPipelineLayout _texturedPipelineLayout = VK_NULL_HANDLE;
        VkPipeline _texturedPipeline = VK_NULL_HANDLE;
        Buffer _cubeVertexBuffer;
        uint32_t _cubeVertexCount = 0;
        uint64_t _whiteTexture = 0;
        VkPipelineLayout _litPipelineLayout = VK_NULL_HANDLE;
        VkPipeline _litPipeline = VK_NULL_HANDLE;
        std::vector<GpuMesh> _meshes;
        std::vector<uint32_t> _freeMeshes;

        static constexpr uint32_t MaterialSets = 256;
        // Surface shaders: push constants (128 bytes), set 0 = XGMaterial UBO, t0..t3 and a wrapping sampler.
        VkSampler _surfaceSampler = VK_NULL_HANDLE;
        VkDescriptorSetLayout _surfaceSetLayout = VK_NULL_HANDLE;
        VkPipelineLayout _surfacePipelineLayout = VK_NULL_HANDLE;
        VkDescriptorPool _materialPool = VK_NULL_HANDLE;
        std::vector<GpuShader> _shaders;
        std::vector<uint32_t> _freeShaders;
        std::vector<GpuMaterial> _materials;
        std::vector<uint32_t> _freeMaterials;
        std::vector<SceneDraw> _sceneDraws;
    };

}
