#pragma once

#include "Pipeline.hpp"
#include "Device.hpp"
#include "GameObject.hpp"
#include "Camera.hpp"
#include "FrameInfo.hpp"

// std
#include <memory>
#include <vector>
#include <stdexcept>
#include <array>

namespace Engine
{
    /**
     * @brief Render system that draws every GameObject with a PointLightComponent as a camera-facing billboard, and updates their data in the GlobalUniformBufferObject.
     *
     * Owns its own VkPipelineLayout and Pipeline (built from
     * point_light.vert/frag.spv with alpha blending enabled and no vertex
     * input, since the quad is generated in the vertex shader from
     * gl_VertexIndex). Per-light data is pushed via vkCmdPushConstants()
     * rather than a vertex buffer.
     */
    class PointLightRenderSystem
    {
    public:
        /**
         * @brief Calls createPipelineLayout() then createPipeline().
         * @param device Device the pipeline layout/pipeline are created on.
         * @param renderPass Render pass the pipeline will be compatible with.
         * @param globalSetLayout Layout of the global (binding-0 uniform buffer) descriptor set, included in the pipeline layout.
         */
        PointLightRenderSystem(Device &device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout);
        /// Calls vkDestroyPipelineLayout() (the Pipeline member destroys the VkPipeline itself).
        ~PointLightRenderSystem();

        PointLightRenderSystem (const PointLightRenderSystem &) = delete;
        PointLightRenderSystem& operator=(const PointLightRenderSystem &) = delete;

        /// Binds the pipeline and global descriptor set, then for each point light (back-to-front sorted by distance to camera) pushes a PointLightPushConstant and calls vkCmdDraw(6 vertices) to draw its billboard quad.
        void render(FrameInfo &frameInfo);
        /// Writes each point-light GameObject's current position/color/intensity into the corresponding slot of ubo.pointLights (no Vulkan calls - the caller uploads ubo afterward).
        void update(FrameInfo &frameInfo, GlobalUniformBufferObject &ubo);
    private:
        /// Calls vkCreatePipelineLayout() with globalSetLayout plus a push-constant range sized for PointLightPushConstant.
        void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
        /// Builds a PipelineConfigInfo (default + alpha blending, no vertex input) and constructs the Pipeline from the point-light shaders.
        void createPipeline(VkRenderPass renderPass);

        Device &device;
        // This lets us create a new swap chain just by creating a new swapChain object
        VkPipelineLayout pipelineLayout;
        std::unique_ptr<Pipeline> pipeline;
    };
}
