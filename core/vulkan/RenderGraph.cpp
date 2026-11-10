#include "RenderGraph.hpp"

#include <iostream>
#include <stdexcept>

namespace
{
    /// Which pipeline stages touch an image while it is in a given layout, and how they access it.
    struct LayoutUsage
    {
        VkPipelineStageFlags stages;
        VkAccessFlags readAccess;
        VkAccessFlags writeAccess;
    };

    LayoutUsage usageForLayout(VkImageLayout layout)
    {
        switch (layout)
        {
            case VK_IMAGE_LAYOUT_UNDEFINED:
                return {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0};
            case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
                return {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT};
            case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL:
                return {VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT};
            case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
                return {VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, 0};
            case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
                // The presentation engine isn't a pipeline stage, and the semaphore passed to
                // vkQueuePresentKHR already makes its read visible.
                return {VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0};
            default:
                // Unknown layout: wait for everything.
                return {VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                        VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_MEMORY_WRITE_BIT};
        }
    }

    /// Layout a pass writes `resource` in: depth attachment for depth images, color attachment otherwise.
    VkImageLayout attachmentLayout(const Engine::RenderResource& resource)
    {
        return (resource.aspectMask & VK_IMAGE_ASPECT_DEPTH_BIT)
            ? VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL
            : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }
}

namespace Engine
{
    RenderGraph::RenderGraph(Device& device) : device{device} {}

    RenderGraph::~RenderGraph()
    {
        for (auto &[name, resource] : resources)
        {
            // Only free resources this graph allocated itself; swapchain-backed
            // resources (memory == VK_NULL_HANDLE) are owned by SwapChain.
            if (resource.memory != VK_NULL_HANDLE)
            {
                vkDestroyImageView(device.device(), resource.imageView, nullptr);
                vkDestroyImage(device.device(), resource.image, nullptr);
                vkFreeMemory(device.device(), resource.memory, nullptr);
            }
        }
    }

    void RenderGraph::addResource(RenderResource renderResource)
    {
        resources[renderResource.name] = renderResource;
    }

    void RenderGraph::addPass(std::unique_ptr<RenderPass> renderPass)
    {
        renderPasses.push_back(std::move(renderPass));
    }

    void RenderGraph::visitNode(
        uint32_t node, 
        std::vector<std::vector<uint32_t>> &dependencies,
        std::vector<NodeStatus> &status, 
        std::vector<uint32_t> &order
    )
    {
        if (status[node] == NodeStatus::VISITED)
        {
            return;
        }
        
        if (status[node] == NodeStatus::VISITING)
        {
            throw std::runtime_error("Cycle detected in render graph: " + renderPasses[node]->getName());
        }

        status[node] = NodeStatus::VISITING;
        for (uint32_t dep : dependencies[node])
        {
            visitNode(dep, dependencies, status, order);
        }

        status[node] = NodeStatus::VISITED;
        order.push_back(node);
    }

    void RenderGraph::notifyResourcesAllocated()
    {
        auto get = [this](const std::string& name) -> const RenderResource& {
            return resources.at(name);
        };

        for(auto& pass : renderPasses)
        {
            pass->onResourcesAllocated(get);
        }
    }

    void RenderGraph::allocateResources()
    {
        for (auto& [name, resource] : resources)
        {
            // Already backed by GPU memory (e.g. resizeResources() only freed some of them).
            if (resource.memory != VK_NULL_HANDLE)
            {
                continue;
            }

            VkImageCreateInfo imageInfo{};
            imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            imageInfo.imageType = VK_IMAGE_TYPE_2D;
            imageInfo.format = resource.format;
            imageInfo.extent = {resource.extent.width, resource.extent.height, 1};
            imageInfo.mipLevels = 1;
            imageInfo.arrayLayers = 1;
            imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
            imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL; // GPU optimal memory layout
            imageInfo.usage = resource.usage;
            imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // Single queue family access
            imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

            device.createImageWithInfo(imageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, resource.image, resource.memory);
            // A freshly created image is in the layout given by imageInfo.initialLayout.
            resource.currentLayout = imageInfo.initialLayout;

            VkImageViewCreateInfo viewInfo{};
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image = resource.image;
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = resource.format;
            viewInfo.subresourceRange = {resource.aspectMask, 0, 1, 0, 1};

            resource.imageView = device.createImageViewWithInfo(viewInfo);

            // Numeric values are the raw Vk enums/bitmasks (look them up in vulkan_core.h).
            std::cout << "resource allocated (name=\"" << resource.name << "\""
                      << ", imageType=" << imageInfo.imageType
                      << ", format=" << imageInfo.format
                      << ", usage=0x" << std::hex << imageInfo.usage << std::dec
                      << ", layout=" << resource.currentLayout
                      << ", extent=" << resource.extent.width << "x" << resource.extent.height
                      << ")" << std::endl;
        }
    }

    void RenderGraph::resizeResources(const std::unordered_map<std::string, VkExtent3D> &newExtents)
    {
        for (const auto &[name, extent] : newExtents)
        {
            auto it = resources.find(name);
            if (it == resources.end())
            {
                continue;
            }

            RenderResource &resource = it->second;
            if (resource.memory != VK_NULL_HANDLE)
            {
                vkDestroyImageView(device.device(), resource.imageView, nullptr);
                vkDestroyImage(device.device(), resource.image, nullptr);
                vkFreeMemory(device.device(), resource.memory, nullptr);
                resource.image = VK_NULL_HANDLE;
                resource.imageView = VK_NULL_HANDLE;
                resource.memory = VK_NULL_HANDLE;
            }
            resource.extent = extent;
        }

        allocateResources();

        notifyResourcesAllocated();
    }

    void RenderGraph::transitionImage(VkCommandBuffer commandBuffer, RenderResource& resource, VkImageLayout newLayout)
    {
        LayoutUsage src = usageForLayout(resource.currentLayout);
        LayoutUsage dst = usageForLayout(newLayout);

        if (resource.currentLayout == VK_IMAGE_LAYOUT_UNDEFINED)
        {
            src.stages = dst.stages;
        }

        // Same layout and the previous user only read it: read-after-read needs no barrier.
        // If anything wrote it (e.g. two passes both drawing into "color"), we still need a
        // barrier even without a layout change, so the second pass sees the first one's writes.
        if (resource.currentLayout == newLayout && src.writeAccess == 0)
        {
            return;
        }

        VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        barrier.oldLayout = resource.currentLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = resource.image;
        barrier.subresourceRange = {resource.aspectMask, 0, 1, 0, 1};
        // Only writes need to be made visible; earlier reads just need to finish (the stage mask handles that).
        barrier.srcAccessMask = src.writeAccess;
        barrier.dstAccessMask = dst.readAccess | dst.writeAccess;

        vkCmdPipelineBarrier(
            commandBuffer,
            src.stages,
            dst.stages,
            0,
            0, nullptr, 0, nullptr, 1, &barrier
        );

        resource.currentLayout = newLayout;
    }

    RenderResource& RenderGraph::findResource(const std::string& name,
                                              std::unordered_map<std::string, RenderResource>& frameResources)
    {
        if (auto it = frameResources.find(name); it != frameResources.end())
        {
            return it->second;
        }
        if (auto it = resources.find(name); it != resources.end())
        {
            return it->second;
        }
        throw std::runtime_error("Render graph resource not found: " + name);
    }

    void RenderGraph::compile()
    {
        // For each render pass holds a list of passes that must run before it.
        std::vector<std::vector<uint32_t>> dependencies(renderPasses.size());

        // Last pass (in addPass order) that wrote each resource so far. Walking the passes in the
        // order they were added means a pass only ever depends on passes added before it, so
        // "passes touching the same resource run in the order they were added".
        std::unordered_map<std::string, uint32_t> lastWriter;

        for (uint32_t i = 0; i < renderPasses.size(); ++i)
        {
            const RenderPass& pass = *renderPasses[i];

            auto dependOnLastWriter = [&](const std::string& name)
            {
                auto it = lastWriter.find(name);
                // it->second != i: a pass listing a resource as both input and output
                // must not depend on itself.
                if (it != lastWriter.end() && it->second != i)
                {
                    dependencies[i].push_back(it->second);
                }
            };

            // Read-after-write: sampling an image another pass rendered.
            for (const auto& input : pass.getInputs())
            {
                dependOnLastWriter(input);
            }
            // Write-after-write: drawing on top of (or clearing) what an earlier pass rendered.
            for (const auto& output : pass.getOutputs())
            {
                dependOnLastWriter(output);
            }
            // Only now record this pass as the writer, so later passes depend on it.
            for (const auto& output : pass.getOutputs())
            {
                lastWriter[output] = i;
            }
        }

        std::vector<NodeStatus> status(renderPasses.size(), NodeStatus::UNVISITED);
        executionOrder.clear();
        for (uint32_t i = 0; i < renderPasses.size(); ++i)
        {
            visitNode(i, dependencies, status, executionOrder);
        }

        // No semaphores between passes: they all record into one command buffer on one queue,
        // where the barriers execute() records already order them.
        allocateResources();

        notifyResourcesAllocated();
    }

    void RenderGraph::execute(VkCommandBuffer& commandBuffer, std::unordered_map<std::string, RenderResource>& frameResources)
    {
        // Iterate through render passes in execution order and synchronize their command execution
        for (auto passIdx : executionOrder)
        {
            // Not const: record() may update the pass's own state (e.g. per-frame descriptor sets).
            RenderPass& currentPass = *renderPasses[passIdx];

            // Move every image this pass samples into a shader-readable layout.
            for (const auto& input : currentPass.getInputs())
            {
                transitionImage(commandBuffer, findResource(input, frameResources),
                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            }

            // Move every image this pass renders into the matching attachment layout.
            for (const auto& output : currentPass.getOutputs())
            {
                auto& resource = findResource(output, frameResources);
                transitionImage(commandBuffer, resource, attachmentLayout(resource));
            }

            RenderPassContext context{};
            context.commandBuffer = commandBuffer;
            context.get = [&](const std::string& name) -> const RenderResource& {return findResource(name, frameResources);};
            currentPass.record(context);
        }

        // Once every pass ran, move each image into the layout its consumer outside the graph expects.
        for (auto* resourceMap : {&resources, &frameResources})
        {
            for (auto& [name, resource] : *resourceMap)
            {
                if (resource.finalLayout != VK_IMAGE_LAYOUT_UNDEFINED)
                {
                    transitionImage(commandBuffer, resource, resource.finalLayout);
                }
            }
        }
    }
}