#include "Renderer.hpp"

namespace Engine
{
    Renderer::Renderer(Window &window, Device &device)
        : window{window}, device{device}
    {
        recreateSwapChain();
        createCommandBuffers();
    }

    Renderer::~Renderer()
    {
        freeCommandBuffers();   
    }

    void Renderer::recreateSwapChain()
    {
        auto extent = window.getExtent();
        // Wait until the swapchain has any size (wait while minimized)
        while(extent.width == 0 || extent.height == 0)
        {
            extent = window.getExtent();
            glfwWaitEvents();
        }

        // Wait for the current swapChain to stop being used.
        vkDeviceWaitIdle(device.device());

        if (swapChain == nullptr)
        {
            swapChain = std::make_unique<SwapChain>(device, extent);
        }
        else
        {
            std::shared_ptr<SwapChain> oldSwapChain = std::move(swapChain);
            swapChain = std::make_unique<SwapChain>(device, extent, oldSwapChain);

            if (!oldSwapChain->compareSwapFormats(*swapChain.get()))
            {
                throw std::runtime_error("Swap chain image (or depth) format has changed!");
            }
        }
        currentFrameIndex = 0;
    }

    void Renderer::createCommandBuffers()
    {
        commandBuffers.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        /***
         * Primary command buffers can be submitted to the device queue but can't be called
         * from other command buffers.
         * Secondary command buffers can be called from other command buffers but can't be
         * submitted to the device queue.
         */
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = device.getCommandPool();
        allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

        if (vkAllocateCommandBuffers(device.device(), &allocInfo, commandBuffers.data()) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to allocate command buffers!");
        }
    }

    void Renderer::freeCommandBuffers()
    {
        vkFreeCommandBuffers(
            device.device(), 
            device.getCommandPool(), 
            static_cast<uint32_t>(commandBuffers.size()), 
            commandBuffers.data()
        );
        commandBuffers.clear();
    }

    VkCommandBuffer Renderer::beginFrame()
    {
        assert(isFrameStarted == false && "Can't begin frame while frame already in progress!");

        // Fetch the index of the image we will render next.
        auto result = swapChain->acquireNextImage(&currentImageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            recreateSwapChain();
            return nullptr;
        }

        if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        {
            throw std::runtime_error("Failed to acquire next swapchain image!");
        }

        isFrameStarted = true;

        auto commandBuffer = getCurrentCommandBuffer();

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to begin recording command buffer!");
        }

        return commandBuffer;

    }

    void Renderer::endFrame()
    {
        assert(isFrameStarted == true && "Can't end while frame not in progress!");
        auto commandBuffer = getCurrentCommandBuffer();
        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to record command buffer!");
        }

        /***
         * Submit the command buffer to device graphics queue while handling CPU-GPU sync.
         * The command buffer will be executed and then the swpachain will present the color
         * attachment image view to the display at the appropriate time based on the present mode.
         */
        auto result = swapChain->submitCommandBuffers(&commandBuffer, &currentImageIndex);

        isFrameStarted = false;
        currentFrameIndex = (currentFrameIndex + 1) % SwapChain::MAX_FRAMES_IN_FLIGHT;

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || window.windowResized())
        {
            window.resetWindowResizeFlag();
            recreateSwapChain();
        }
        else if (result != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to present swapchain image.");
        }
    }

    void Renderer::beginRenderPass(VkCommandBuffer commandBuffer)
    {
        assert(isFrameStarted == true && "Can't begin render pass while frame not in progress!");
        assert(commandBuffer == getCurrentCommandBuffer() && "Can't begin render pass on a command buffer from different frame");

        // Begin a render pass
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = swapChain->getRenderPass();
        renderPassInfo.framebuffer = swapChain->getFrameBuffer(currentImageIndex);
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = swapChain->getSwapChainExtent();

        /**
         * Clear values is an array indexed by attachment index. 
         * If an attachment uses LOAD_OP_LOAD or DONT_CARE, its corresponding VkClearValue entry is simply unused.
         */
        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = {0.01f, 0.01f, 0.01f, 1.0f};
        clearValues[1].depthStencil = {1.0f, 0};

        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        // After beginning a render pass instance, the command buffer is ready to 
        // record the commands for the first subpass of that render pass.
        // VK_SUBPASS_CONTENTS_INLINE: no secondary command buffer will be used.
        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        // Configure dynamic viewport and scissor
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        viewport.width = static_cast<uint32_t>(swapChain->getSwapChainExtent().width);
        viewport.height = static_cast<uint32_t>(swapChain->getSwapChainExtent().height);

        VkRect2D scissor{};
        scissor.extent = swapChain->getSwapChainExtent();
        scissor.offset = {0,0};

        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    }

    void Renderer::endRenderPass(VkCommandBuffer commandBuffer)
    {
        assert(isFrameStarted == true && "Can't end render pass while frame not in progress!");
        assert(commandBuffer == getCurrentCommandBuffer() && "Can't end render pass on a command buffer from different frame");

        vkCmdEndRenderPass(commandBuffer);
    }
}