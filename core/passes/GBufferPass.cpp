#include "GBufferPass.hpp"

namespace
{
    /// Number of G-buffer color targets: albedo (location 0), normal (1), position (2).
    constexpr uint32_t COLOR_ATTACHMENT_COUNT = 3;

    constexpr VkClearColorValue ALBEDO_CLEAR_COLOR{{0.01f, 0.01f, 0.01f, 1.0f}};
    /// Alpha 0 marks background pixels; the shader writes alpha 1 wherever geometry is drawn.
    constexpr VkClearColorValue NORMAL_CLEAR_COLOR{{0.0f, 0.0f, 0.0f, 0.0f}};
    constexpr VkClearColorValue POSITION_CLEAR_COLOR{{0.0f, 0.0f, 0.0f, 0.0f}};

    /// 1.0 is the far plane, so every fragment passes the LESS depth test against a freshly cleared buffer.
    constexpr VkClearDepthStencilValue DEPTH_CLEAR_VALUE{1.0f, 0};

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

    VkRenderingAttachmentInfo createDepthAttachment(VkImageView view)
    {
        VkRenderingAttachmentInfo attachment{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
        attachment.imageView = view;
        attachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachment.clearValue.depthStencil = DEPTH_CLEAR_VALUE;
        return attachment;
    }
}

namespace Engine
{
    GBufferPass::GBufferPass(FrameInfo& frameInfo, SolidRenderSystem& solid) 
        : RenderPass("gbuffer", {}, {"albedo", "normal", "position", "depth"}), 
        frameInfo{frameInfo}, solid{solid}
    {
    }

    void GBufferPass::record(RenderPassContext& ctx)
    {   
        const RenderResource& albedo    = ctx.get("albedo");
        const RenderResource& normal    = ctx.get("normal");
        const RenderResource& position  = ctx.get("position");
        const RenderResource& depth     = ctx.get("depth");

        // Order must match the shader's layout(location = N) outputs.
        VkRenderingAttachmentInfo colorAttachments[COLOR_ATTACHMENT_COUNT] = {
            createAttachment(albedo.imageView, ALBEDO_CLEAR_COLOR),
            createAttachment(normal.imageView, NORMAL_CLEAR_COLOR),
            createAttachment(position.imageView, POSITION_CLEAR_COLOR)
        };

        VkRenderingAttachmentInfo depthAttachment = createDepthAttachment(depth.imageView);

        beginDynamicRendering(
            ctx.commandBuffer,
            {albedo.extent.width, albedo.extent.height},
            COLOR_ATTACHMENT_COUNT,
            colorAttachments,
            &depthAttachment
        );

        solid.renderGameObjects(frameInfo);

        vkCmdEndRendering(ctx.commandBuffer);
    }
}