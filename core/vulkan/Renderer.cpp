#include "Renderer.hpp"

namespace Engine
{
    Renderer::Renderer(Window &window, Device &device)
        : window{window}, device{device}
    {
        // Created first: recreateSwapChain() resizes the graph's screen-sized resources.
        renderGraph = std::make_unique<RenderGraph>(device);
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
                throw std::runtime_error("Swap chain image format has changed!");
            }
        }
        currentFrameIndex = 0;

        // Every attachment of a pass must cover its render area, so screen-sized render targets
        // follow the new swap chain extent. Safe to destroy the old images: vkDeviceWaitIdle() ran above.
        VkExtent2D swapChainExtent = swapChain->getSwapChainExtent();
        std::unordered_map<std::string, VkExtent3D> newExtents;
        for (const auto& name : screenSizedResources)
        {
            newExtents[name] = {swapChainExtent.width, swapChainExtent.height, 1};
        }
        renderGraph->resizeResources(newExtents);
    }

    void Renderer::addScreenSizedResource(RenderResource resource)
    {
        VkExtent2D swapChainExtent = swapChain->getSwapChainExtent();
        resource.extent = {swapChainExtent.width, swapChainExtent.height, 1};
        screenSizedResources.push_back(resource.name);
        renderGraph->addResource(resource);
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

    void Renderer::executeRenderGraph(VkCommandBuffer commandBuffer)
    {
        assert(isFrameStarted && "Can't execute the render graph while frame not in progress");

        VkExtent2D currentImageExtent = swapChain->getSwapChainExtent();
        RenderResource backBuffer{};
        backBuffer.name = "backbuffer";
        backBuffer.image = swapChain->getImage(currentImageIndex);
        backBuffer.imageView = swapChain->getImageView(currentImageIndex);
        backBuffer.format = swapChain->getSwapChainImageFormat();
        backBuffer.extent = {currentImageExtent.width, currentImageExtent.height, 1};
        backBuffer.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        backBuffer.currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        backBuffer.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        std::unordered_map<std::string, RenderResource> frameResources {{"backbuffer", backBuffer}};
        renderGraph->execute(commandBuffer, frameResources);
    }
}