#include "SolidPass.hpp"

namespace
{
    /// Color attachment that is cleared at vkCmdBeginRendering() and kept (STORE) afterwards.
    VkRenderingAttachmentInfo colorAttachment(VkImageView view, VkClearColorValue clearColor)
    {
        VkRenderingAttachmentInfo attachment{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
        attachment.imageView = view;
        attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachment.clearValue.color = clearColor;
        return attachment;
    }

    /// Depth attachment cleared to 1.0 (far plane); contents are discarded (DONT_CARE) since nothing reads them afterwards.
    VkRenderingAttachmentInfo depthAttachment(VkImageView view)
    {
        VkRenderingAttachmentInfo attachment{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
        attachment.imageView = view;
        attachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachment.clearValue.depthStencil = {1.0f, 0};
        return attachment;
    }
}

namespace Engine
{
    SolidPass::SolidPass(FrameInfo& frameInfo, SolidRenderSystem& solid)
        : RenderPass{"solid", {}, {"backbuffer", "depth"}},
        frameInfo{frameInfo}, solid{solid}
    {
    }

    void SolidPass::record(RenderPassContext& ctx)
    {
        const RenderResource& backbuffer = ctx.get("backbuffer");
        const RenderResource& depth = ctx.get("depth");

        VkRenderingAttachmentInfo colorInfo = colorAttachment(backbuffer.imageView, {{0.01f, 0.01f, 0.01f, 1.0f}});
        VkRenderingAttachmentInfo depthInfo = depthAttachment(depth.imageView);

        beginDynamicRendering(
            ctx.commandBuffer, 
            {backbuffer.extent.width, backbuffer.extent.height},
            1, &colorInfo, &depthInfo
        );

        solid.renderGameObjects(frameInfo);

        vkCmdEndRendering(ctx.commandBuffer);
    }
}