#pragma once

#include "Window.hpp"
#include "Device.hpp"
#include "SwapChain.hpp"
#include "RenderGraph.hpp"

// std
#include <memory>
#include <vector>
#include <cassert>

namespace Engine
{
    /**
     * @brief Owns the per-frame command buffers, the SwapChain and the RenderGraph, and drives the acquire/record/submit/present cycle.
     *
     * This is the layer that turns "begin a frame" / "end a frame" into the
     * underlying vkAcquireNextImageKHR -> vkBeginCommandBuffer ->
     * RenderGraph::execute() (barriers + every pass) -> vkEndCommandBuffer
     * -> vkQueueSubmit -> vkQueuePresentKHR sequence, and recreates the
     * SwapChain (and resizes the graph's screen-sized resources) when it goes
     * out of date (e.g. on window resize).
     */
    class Renderer
    {
    public:
        /**
         * @brief Builds the initial SwapChain and allocates one command buffer per frame-in-flight.
         * @param window Window the swap chain presents to and whose resize events are watched.
         * @param device Device the swap chain and command buffers are created on.
         */
        Renderer(Window &window, Device &device);
        /// Calls vkFreeCommandBuffers() for the frame command buffers (the SwapChain destroys itself via its own destructor).
        ~Renderer();

        Renderer (const Renderer &) = delete;
        Renderer& operator=(const Renderer &) = delete;


        /**
         * @brief Acquires the next swap chain image (vkAcquireNextImageKHR) and calls vkBeginCommandBuffer() on this frame's command buffer.
         * @return The command buffer ready for recording, or nullptr if the swap chain was out of date and had to be recreated (caller should skip the frame).
         */
        VkCommandBuffer beginFrame();
        /// Calls vkEndCommandBuffer(), then SwapChain::submitCommandBuffers() (vkQueueSubmit + vkQueuePresentKHR); recreates the swap chain if it reports out-of-date/suboptimal or the window was resized.
        void endFrame();
        /// @return true between a beginFrame()/endFrame() pair.
        bool isFrameInProgress() const {return isFrameStarted;}

        /// @return The current swap chain extent's width/height ratio, for building the camera's perspective projection.
        float getAspectRatio() const
        {
            return swapChain->extentAspectRatio();
        }

        /// @return The command buffer for the frame currently in progress.
        VkCommandBuffer getCurrentCommandBuffer() const
        {
            assert(isFrameStarted == true && "Can't get frame buffer while frame not in progress!");
            return commandBuffers[currentFrameIndex];
        }


        /// @return Index of the current frame-in-flight (0..SwapChain::MAX_FRAMES_IN_FLIGHT-1), used to index per-frame resources like the uniform buffers.
        int getFrameIndex() const
        {
            assert(isFrameStarted == true && "Can't get frame index when frame not in progress!");
            return currentFrameIndex;
        }

        VkFormat getColorAttachmentFormat() { return swapChain->getSwapChainImageFormat(); }
        VkFormat getDepthAttachmentFormat() { return swapChain->findDepthFormat(); }

        RenderGraph& getGraph() { return *renderGraph; }

        /// Adds `resource` to the render graph with its extent set to the swap chain extent, and resizes it with the swap chain from then on (in recreateSwapChain()).
        void addScreenSizedResource(RenderResource resource);

        /// @brief Fills the frame resources for the current image being drawn and executes the render passes inside the render graph.
        /// @param commandBuffer 
        void executeRenderGraph(VkCommandBuffer commandBuffer);

    private:
        /// Calls vkAllocateCommandBuffers() for MAX_FRAMES_IN_FLIGHT primary command buffers.
        void createCommandBuffers();
        /// Calls vkFreeCommandBuffers() and clears commandBuffers.
        void freeCommandBuffers();
        /// Waits for the window to have a non-zero size, calls vkDeviceWaitIdle(), then constructs a new SwapChain (chained off the old one via its "previous" constructor) to replace the stale one.
        void recreateSwapChain();

        Window &window;
        Device &device;
        std::unique_ptr<SwapChain> swapChain;
        std::vector<VkCommandBuffer> commandBuffers;
        std::unique_ptr<RenderGraph> renderGraph;
        // Names of graph resources that always match the swap chain extent (depth, later the G-buffer).
        std::vector<std::string> screenSizedResources;

        uint32_t currentImageIndex = 0;
        int currentFrameIndex = 0;
        bool isFrameStarted = false;
    };
}
