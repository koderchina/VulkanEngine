#include "RenderPass.hpp"

#include <utility>

namespace Engine
{
    void beginDynamicRendering(VkCommandBuffer commandBuffer, VkExtent2D extent,
                               uint32_t colorCount, const VkRenderingAttachmentInfo* colorAttachments,
                               const VkRenderingAttachmentInfo* depthAttachment)
    {
        VkRenderingInfo renderInfo{VK_STRUCTURE_TYPE_RENDERING_INFO};
        renderInfo.renderArea.offset = {0, 0};
        renderInfo.renderArea.extent = extent;
        renderInfo.layerCount = 1;
        renderInfo.colorAttachmentCount = colorCount;
        renderInfo.pColorAttachments = colorAttachments;
        renderInfo.pDepthAttachment = depthAttachment;

        vkCmdBeginRendering(commandBuffer, &renderInfo);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(extent.width);
        viewport.height = static_cast<float>(extent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = extent;

        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    }

    RenderPass::RenderPass(std::string name, std::vector<std::string> inputs, std::vector<std::string> outputs)
        : name{std::move(name)}, inputs{std::move(inputs)}, outputs{std::move(outputs)}
    {
    }
}
