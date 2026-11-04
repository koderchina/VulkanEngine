#include "SwapChain.hpp"

// std
#include <array>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>

namespace Engine
{
    SwapChain::SwapChain(Device &deviceRef, VkExtent2D windowExtent) 
        : device{deviceRef}, windowExtent{windowExtent} 
    {
        init();
    }

    SwapChain::SwapChain(Device &deviceRef, VkExtent2D windowExtent, std::shared_ptr<SwapChain> previous) 
        : device{deviceRef}, windowExtent{windowExtent}, oldSwapChain{previous} 
    {
        init();

        // no longer needed, can be deallocated.
        oldSwapChain = nullptr;
    }

    void SwapChain::init()
    {
        createSwapChain();
        createSyncObjects();
    }

    SwapChain::~SwapChain()
    {
        for (auto imageView : swapChainImageViews)
        {
            vkDestroyImageView(device.device(), imageView, nullptr);
        }
        swapChainImageViews.clear();

        if (swapChain != nullptr)
        {
            vkDestroySwapchainKHR(device.device(), swapChain, nullptr);
            swapChain = nullptr;
        }

        for(int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            vkDestroySemaphore(device.device(), renderFinishedSemaphores[i], nullptr);
            vkDestroySemaphore(device.device(), imageAvailableSemaphores[i], nullptr);
            vkDestroyFence(device.device(), inFlightFences[i], nullptr);
        }
    }

    VkResult SwapChain::acquireNextImage(uint32_t *imageIndex) 
    {
        vkWaitForFences(
            device.device(),
            1,
            &inFlightFences[currentFrame],
            VK_TRUE,
            std::numeric_limits<uint64_t>::max());

        VkResult result = vkAcquireNextImageKHR(
            device.device(),
            swapChain,
            std::numeric_limits<uint64_t>::max(),
            imageAvailableSemaphores[currentFrame],  // must be a not signaled semaphore
            VK_NULL_HANDLE,
            imageIndex);

        return result;
    }

    VkResult SwapChain::submitCommandBuffers(const VkCommandBuffer *buffers, uint32_t *imageIndex) 
    {
        if (imagesInFlight[*imageIndex] != VK_NULL_HANDLE) 
        {
            vkWaitForFences(device.device(), 1, &imagesInFlight[*imageIndex], VK_TRUE, UINT64_MAX);
        }
        imagesInFlight[*imageIndex] = inFlightFences[currentFrame];

        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;

        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = buffers;

        VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        vkResetFences(device.device(), 1, &inFlightFences[currentFrame]);
        if (vkQueueSubmit(device.graphicsQueue(), 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) 
        {
            throw std::runtime_error("failed to submit draw command buffer!");
        }

        VkPresentInfoKHR presentInfo = {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;

        VkSwapchainKHR swapChains[] = {swapChain};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;

        presentInfo.pImageIndices = imageIndex;

        auto result = vkQueuePresentKHR(device.presentQueue(), &presentInfo);

        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

        return result;
    }

    /**
     * vkb::SwapchainBuilder queries the surface (vkGetPhysicalDeviceSurfaceCapabilities/Formats/PresentModesKHR),
     * picks the format, present mode, extent and image count, sets the sharing mode from the
     * device's graphics/present queue families, and calls vkCreateSwapchainKHR.
     * - Format: B8G8R8A8_SRGB if available, otherwise the first one the surface reports.
     * - Present mode: FIFO (V-Sync), which every implementation must support.
     * - Extent: the surface's currentExtent, or windowExtent clamped to min/maxImageExtent.
     * - Image count: minImageCount + 1, clamped to maxImageCount.
     */
    void SwapChain::createSwapChain()
    {
        vkb::SwapchainBuilder builder{device.vkbDevice()};
        auto result = builder
            .set_desired_format({VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
            // Try VK_PRESENT_MODE_MAILBOX_KHR or IMMEDIATE_KHR here to disable V-Sync;
            // vk-bootstrap falls back to FIFO if the surface doesn't support it.
            .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
            .set_desired_extent(windowExtent.width, windowExtent.height)
            .set_image_usage_flags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
            .set_old_swapchain(oldSwapChain == nullptr ? VK_NULL_HANDLE : oldSwapChain->swapChain)
            .build();
        if (!result)
        {
            throw std::runtime_error("failed to create swap chain: " + result.error().message());
        }
        std::cout << "Present mode: V-Sync" << std::endl;

        vkb::Swapchain vkbSwapchain = result.value();
        swapChain = vkbSwapchain.swapchain;
        swapChainImageFormat = vkbSwapchain.image_format;
        swapChainExtent = vkbSwapchain.extent;

        // we only specified a minimum number of images in the swap chain, so the implementation is
        // allowed to create a swap chain with more. get_images() is vkGetSwapchainImagesKHR (count, then handles).
        // get_image_views() is one 2D, color-aspect vkCreateImageView per image.
        // The views are destroyed manually in ~SwapChain.
        swapChainImages = vkbSwapchain.get_images().value();
        swapChainImageViews = vkbSwapchain.get_image_views().value();
    }

    void SwapChain::createSyncObjects() 
    {
        imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
        imagesInFlight.resize(imageCount(), VK_NULL_HANDLE);

        VkSemaphoreCreateInfo semaphoreInfo = {};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo = {};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) 
        {
            if (vkCreateSemaphore(device.device(), &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) !=
                    VK_SUCCESS ||
                vkCreateSemaphore(device.device(), &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) !=
                    VK_SUCCESS ||
                vkCreateFence(device.device(), &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) 
            {
                throw std::runtime_error("failed to create synchronization objects for a frame!");
            }
        }
    }

    VkFormat SwapChain::findDepthFormat() {
        return device.findSupportedFormat(
            {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
    }
}