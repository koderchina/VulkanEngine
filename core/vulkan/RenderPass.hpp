#pragma once

#include "RenderResource.hpp"

#include <vulkan/vulkan.h>

#include <functional>
#include <string>
#include <vector>
#include <iostream>

namespace Engine
{
    /// Calls vkCmdBeginRendering() over `extent` with the given attachments, then sets a full-extent viewport/scissor (dynamic state in every pipeline).
    /// @param depthAttachment nullptr when the pass has no depth attachment.
    void beginDynamicRendering(VkCommandBuffer commandBuffer, VkExtent2D extent,
                               uint32_t colorCount, const VkRenderingAttachmentInfo* colorAttachments,
                               const VkRenderingAttachmentInfo* depthAttachment);

    /// What RenderGraph::execute() hands a pass while it records: the frame's command buffer and a lookup for the current image/view of any resource.
    struct RenderPassContext
    {
        VkCommandBuffer commandBuffer;
        std::function<const RenderResource&(const std::string&)> get;
    };

    /**
     * @brief Base class for one node of the RenderGraph.
     *
     * A subclass names the resources it samples (inputs) and renders into
     * (outputs) through the constructor, and records  
     * its commands in record().
     * The graph uses inputs/outputs to order the passes and to record every
     * image barrier before calling record(), so record() never records barriers itself.
     */
    class RenderPass
    {
    public:
        virtual ~RenderPass() = default;

        /**
         * Called by the graph after it (re)allocates its images: once at the end of compile() and again after
         * every resizeResources(). Override it to refresh anything that holds an image view, e.g. a descriptor set.
         * Does nothing by default.
         * @param get Looks a resource up by name; its imageView is the newly created one.
         *            Don't ask for "backbuffer": the swap chain image is only supplied per frame, in execute().
         */
        virtual void onResourcesAllocated(const std::function<const RenderResource&(const std::string&)>& get)
        {
            std::cout << "[" << name << "]: " << "Resources allocated..." << std::endl;
        }

        RenderPass(const RenderPass &) = delete;
        RenderPass& operator=(const RenderPass &) = delete;

        const std::string& getName() const { return name; }
        /// Resources this pass samples; the graph moves them to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL first.
        const std::vector<std::string>& getInputs() const { return inputs; }
        /// Resources this pass renders into; the graph moves them to the matching attachment layout first.
        const std::vector<std::string>& getOutputs() const { return outputs; }

        /// Records this pass's commands (vkCmdBeginRendering ... draws ... vkCmdEndRendering) into ctx.commandBuffer.
        virtual void record(RenderPassContext& ctx) = 0;

    protected:
        RenderPass(std::string name, std::vector<std::string> inputs, std::vector<std::string> outputs);

    private:
        // Human readable identifier
        std::string name;
        std::vector<std::string> inputs;
        std::vector<std::string> outputs;
    };
}
