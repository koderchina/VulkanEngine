#include "LightingPass.hpp"

namespace
{
    constexpr VkClearColorValue BACKBUFFER_CLEAR_COLOR{{0.01f, 0.01f, 0.01f, 1.0f}};

    VkRenderingAttachmentInfo createAttachment(VkImageView view, VkClearColorValue clearColor)
    {
        VkRenderingAttachmentInfo attachment{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
        attachment.imageView = view;
        attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachment.clearValue.color = clearColor;
        return attachment;
    }
}

namespace Engine
{
    LightingPass::LightingPass(FrameInfo& frameInfo, LightingSystem& lightingSystem) 
        : RenderPass("lightpass", {"albedo", "normal", "position"}, {"backbuffer"}),
        frameInfo{frameInfo}, lightingSystem{lightingSystem}
    {
    }

    void LightingPass::record(RenderPassContext& ctx)
    {
        const RenderResource& backbuffer = ctx.get("backbuffer");

        VkRenderingAttachmentInfo colorAttachmentInfo = createAttachment(
            backbuffer.imageView, 
            BACKBUFFER_CLEAR_COLOR
        );

        beginDynamicRendering(
            ctx.commandBuffer,
            {backbuffer.extent.width, backbuffer.extent.height},
            1,
            &colorAttachmentInfo,
            nullptr
        );

        lightingSystem.render(frameInfo);

        vkCmdEndRendering(ctx.commandBuffer);
    }

    void LightingPass::onResourcesAllocated(const std::function<const RenderResource&(const std::string&)>& get)
    {
        lightingSystem.updateGBuffer(
            get("albedo").imageView,
            get("normal").imageView,
            get("position").imageView
        );

    }
}