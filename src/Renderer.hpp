#pragma once

#include "Window.hpp"
#include "Device.hpp"
#include "SwapChain.hpp"

// std
#include <memory>
#include <vector>
#include <cassert>

namespace Engine
{
    class Renderer
    {
    public:
        Renderer(Window &window, Device &device);
        ~Renderer();

        Renderer (const Renderer &) = delete;
        Renderer& operator=(const Renderer &) = delete;

        
        VkCommandBuffer beginFrame();
        void endFrame();
        bool isFrameInProgress() const {return isFrameStarted;}

        VkRenderPass getRenderPass() const
        {
            return swapChain->getRenderPass();
        }

        float getAspectRatio() const
        {
            return swapChain->extentAspectRatio();
        }

        VkCommandBuffer getCurrentCommandBuffer() const
        {
            assert(isFrameStarted == true && "Can't get frame buffer while frame not in progress!");
            return commandBuffers[currentFrameIndex];
        }

        void beginRenderPass(VkCommandBuffer commandBuffer);
        void endRenderPass(VkCommandBuffer commandBuffer);

        int getFrameIndex() const
        {
            assert(isFrameStarted == true && "Can't get frame index when frame not in progress!");
            return currentFrameIndex;
        }

    private:
        void createCommandBuffers();
        void freeCommandBuffers();
        void recreateSwapChain();

        Window &window;
        Device &device;
        std::unique_ptr<SwapChain> swapChain; 
        std::vector<VkCommandBuffer> commandBuffers;

        uint32_t currentImageIndex = 0;
        int currentFrameIndex = 0;
        bool isFrameStarted = false;
    };
}