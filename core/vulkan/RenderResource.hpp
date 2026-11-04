#pragma once

#include <vulkan/vulkan.h>

#include <string>

namespace Engine
{
    /**
     * @brief Tracks a single image (swapchain attachment or graph-owned render target) across passes.
     *
     * Holds raw, non-owning handles plus the one piece of mutable state the
     * graph actually needs: currentLayout. Execute() compares each pass's
     * required layout for this resource against currentLayout to decide
     * whether a vkCmdPipelineBarrier is needed, then updates currentLayout
     * afterwards. For swapchain-backed resources, image/imageView/currentLayout
     * are refreshed in place once per frame from SwapChain (whoever owns the
     * image keeps owning it); memory stays VK_NULL_HANDLE for those. For a
     * resource the graph allocates itself (e.g. a shadow map), memory is set
     * so the destructor knows it must free image/imageView/memory itself.
     */
    struct RenderResource
    {
        // Human readable identifier
        std::string name;

        VkImage image {VK_NULL_HANDLE};
        VkImageView imageView {VK_NULL_HANDLE};
        VkFormat format {VK_FORMAT_UNDEFINED};
        VkExtent3D extent {};
        VkImageAspectFlags aspectMask {VK_IMAGE_ASPECT_COLOR_BIT};
        VkImageUsageFlags usage{};
        // Layout the image is in right now. Every barrier execute() records reads and updates it,
        // so it carries over between passes (and between frames for graph-owned resources).
        VkImageLayout currentLayout {VK_IMAGE_LAYOUT_UNDEFINED};
        // Layout execute() leaves the image in once all passes ran (e.g. PRESENT_SRC_KHR).
        // VK_IMAGE_LAYOUT_UNDEFINED = no requirement, leave it in whatever the last pass used.
        VkImageLayout finalLayout {VK_IMAGE_LAYOUT_UNDEFINED};
        // VK_NULL_HANDLE for resources this graph doesn't own (swapchain
        // attachments) - only non-null for resources it allocated itself.
        VkDeviceMemory memory {VK_NULL_HANDLE};
    };
}
