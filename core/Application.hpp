#pragma once

#include "Window.hpp"
#include "Device.hpp"
#include "Renderer.hpp"
#include "GameObject.hpp"
#include "Image.hpp"
#include "Descriptors.hpp"
// std
#include <vector>
#include <unordered_map>

namespace Engine
{
    /**
     * @brief Top-level owner of the Window/Device/Renderer and the scene's GameObjects; run() is the main loop.
     *
     * Wires together every other engine class: Window and Device are
     * constructed first (member init order), then Renderer (which owns the
     * SwapChain), then a DescriptorPool used by run() to allocate the
     * per-frame global uniform buffer descriptor sets.
     */
    class Application
    {
    public:
        static constexpr int16_t WIDTH = 1280;
        static constexpr int16_t HEIGHT = 720;
        /// Builds the global DescriptorPool and calls loadGameObjects() to populate the scene.
        Application();
        ~Application();

        Application (const Application &) = delete;
        Application& operator=(const Application &) = delete;

        /// Creates the per-frame uniform buffers/descriptor sets and render systems, then runs the main loop (poll input -> update camera -> Renderer::beginFrame() -> executeRenderGraph() -> endFrame()) until the window is closed, finishing with vkDeviceWaitIdle().
        void run();
    private:
        /// Populates `gameObjects` with the vase/floor models (via Model::createModelFromFile()) and a ring of point lights (via GameObject::makePointLight()).
        void loadGameObjects();
        void loadTextures();

        Window window{WIDTH, HEIGHT, "Vulkan Engine Window"};
        Device device{window};
        Renderer renderer{window, device};

        std::unique_ptr<DescriptorPool> globalPool{};

        // Binds texture path to the images in the GPU VRAM
        std::unordered_map<std::string, std::unique_ptr<Image>> textureIndex{};

        // Binds texture path to the descriptor set for that texture
        std::unordered_map<std::string, VkDescriptorSet> materialDescriptorSets{};
        
        GameObject::Map gameObjects;

    };
}
