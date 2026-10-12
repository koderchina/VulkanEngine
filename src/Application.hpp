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
    class Application
    {
    public:
        static constexpr int16_t WIDTH = 1920;
        static constexpr int16_t HEIGHT = 1080;
        Application();
        ~Application();

        Application (const Application &) = delete;
        Application& operator=(const Application &) = delete;

        void run();
    private:
        void sierpinski(
            std::vector<Model::Vertex> &vertices, 
            uint32_t depth,
            glm::vec2 left, glm::vec2 right, glm::vec2 top
        );
        void loadGameObjects();

        Window window{WIDTH, HEIGHT, "Vulkan Engine Window"};
        Device device{window};
        Renderer renderer{window, device};

        std::unique_ptr<DescriptorPool> globalPool{};
        GameObject::Map gameObjects;
    };
}