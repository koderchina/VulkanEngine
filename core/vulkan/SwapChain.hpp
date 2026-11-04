#pragma once

#include "Device.hpp"

#include <vulkan/vulkan.h>

#include <memory>
#include <string>
#include <vector>

namespace Engine
{
    /**
     * @brief Owns the VkSwapchainKHR, its images/views and per-frame sync objects (semaphores/fences).
     *
     * The depth buffer is not here: it is a screen-sized RenderGraph resource (see Renderer::addScreenSizedResource()).
     *
     * Everything the renderer needs to acquire an image, render into it and
     * present it lives here. When the window resizes or the swap chain
     * reports out-of-date, Renderer builds a new SwapChain passing the old
     * one in as `previous` so vkb::SwapchainBuilder can recycle it
     * (VkSwapchainCreateInfoKHR::oldSwapchain) during vkCreateSwapchainKHR().
     */
    class SwapChain
    {
    public:
        /// Number of frames allowed to be in flight on the GPU simultaneously; sizes the sync object and command buffer pools.
        static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

        /**
         * @brief Calls init() to build a fresh swap chain and sync objects.
         * @param deviceRef Device the swap chain is created on.
         * @param windowExtent Desired swap chain extent (usually the window's framebuffer size).
         */
        SwapChain(Device &deviceRef, VkExtent2D windowExtent);
        /**
         * @brief Same as the two-argument constructor, but passes `previous`'s VkSwapchainKHR as oldSwapchain so it can be reused/recycled during creation.
         * @param deviceRef Device the swap chain is created on.
         * @param windowExtent Desired swap chain extent.
         * @param previous The swap chain being replaced (e.g. after a window resize).
         */
        SwapChain(Device &deviceRef, VkExtent2D windowExtent, std::shared_ptr<SwapChain> previous);
        /// Destroys image views, the swap chain and the sync objects, in that order.
        ~SwapChain();

        SwapChain(const SwapChain &) = delete;
        SwapChain& operator=(const SwapChain &) = delete;

        /// @return The VkImage for swap chain image `index`.
        VkImage getImage(int index) {return swapChainImages[index]; }
        /// @return The VkImageView for swap chain image `index`.
        VkImageView getImageView(int index) { return swapChainImageViews[index]; }
        /// @return Number of images in the swap chain.
        size_t imageCount() { return swapChainImages.size(); }
        /// @return The swap chain's chosen VkFormat (e.g. VK_FORMAT_B8G8R8A8_SRGB).
        VkFormat getSwapChainImageFormat() { return swapChainImageFormat; }
        /// @return The swap chain's VkExtent2D.
        VkExtent2D getSwapChainExtent() { return swapChainExtent; }
        /// @return Swap chain width in pixels.
        uint32_t width() { return swapChainExtent.width; }
        /// @return Swap chain height in pixels.
        uint32_t height() { return swapChainExtent.height; }

        /// @return width/height of the swap chain extent, for the camera's perspective projection.
        float extentAspectRatio() {
            return static_cast<float>(swapChainExtent.width) / static_cast<float>(swapChainExtent.height);
        }
        /// @return The first supported depth format from a fixed candidate list (via Device::findSupportedFormat()).
        VkFormat findDepthFormat();

        /**
         * @brief Waits on the in-flight fence for this frame, then calls vkAcquireNextImageKHR() to get the next presentable image.
         * @param imageIndex Out parameter receiving the acquired image's index.
         * @return VK_SUCCESS, VK_SUBOPTIMAL_KHR, VK_ERROR_OUT_OF_DATE_KHR, or another VkResult from vkAcquireNextImageKHR().
         */
        VkResult acquireNextImage(uint32_t *imageIndex);
        /**
         * @brief Waits on any fence already using this image, then calls vkQueueSubmit() with the frame's wait/signal semaphores and vkQueuePresentKHR() to show the result.
         * @param buffers Pointer to the single command buffer to submit for this frame.
         * @param imageIndex Pointer to the image index previously returned by acquireNextImage().
         * @return VkResult of vkQueuePresentKHR().
         */
        VkResult submitCommandBuffers(const VkCommandBuffer *buffers, uint32_t *imageIndex);

        /// @return true if `swapChain`'s color format matches this swap chain's (used to detect a format change across a swap chain recreation).
        /// The depth format isn't compared: it comes from the device (findDepthFormat()), so recreating the swap chain can't change it.
        bool compareSwapFormats(const SwapChain &swapChain) const
        {
            return swapChain.swapChainImageFormat == swapChainImageFormat;
        }

    private:
        /// Runs the full creation sequence: createSwapChain(), createSyncObjects().
        void init();
        void createSwapChain();     // vkCreateSwapchainKHR + image views (vkb::SwapchainBuilder)
        /// Calls vkCreateSemaphore() (image-available, render-finished) and vkCreateFence() (in-flight) for each frame-in-flight.
        void createSyncObjects();

        VkFormat swapChainImageFormat;
        VkExtent2D swapChainExtent;

        std::vector<VkImage> swapChainImages;
        std::vector<VkImageView> swapChainImageViews;

        Device &device;
        VkExtent2D windowExtent;

        VkSwapchainKHR swapChain;
        std::shared_ptr<SwapChain> oldSwapChain;

        std::vector<VkSemaphore> imageAvailableSemaphores;
        std::vector<VkSemaphore> renderFinishedSemaphores;
        std::vector<VkFence> inFlightFences;
        std::vector<VkFence> imagesInFlight;
        size_t currentFrame = 0;
    };
}
