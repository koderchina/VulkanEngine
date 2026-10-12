#pragma once

#include "Window.hpp"
#include "Device.hpp"
#include "Renderer.hpp"
#include "GameObject.hpp"
#include "Descriptors.hpp"
// std
#include <vector>

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
        static constexpr int16_t WIDTH = 1920;
        static constexpr int16_t HEIGHT = 1080;
        /// Builds the global DescriptorPool and calls loadGameObjects() to populate the scene.
        Application();
        ~Application();

        Application (const Application &) = delete;
        Application& operator=(const Application &) = delete;

        /// Creates the per-frame uniform buffers/descriptor sets and render systems, then runs the main loop (poll input -> update camera -> Renderer::beginFrame()/beginRenderPass() -> render systems -> endRenderPass()/endFrame()) until the window is closed, finishing with vkDeviceWaitIdle().
        void run();
    private:
        /// Recursively subdivides a triangle into a Sierpinski fractal, appending generated vertices (unused by the current scene, kept from an earlier exercise).
        void sierpinski(
            std::vector<Model::Vertex> &vertices,
            uint32_t depth,
            glm::vec2 left, glm::vec2 right, glm::vec2 top
        );
        /// Populates `gameObjects` with the vase/floor models (via Model::createModelFromFile()) and a ring of point lights (via GameObject::makePointLight()).
        void loadGameObjects();

        Window window{WIDTH, HEIGHT, "Vulkan Engine Window"};
        Device device{window};
        Renderer renderer{window, device};

        std::unique_ptr<DescriptorPool> globalPool{};
        GameObject::Map gameObjects;
    };
}
