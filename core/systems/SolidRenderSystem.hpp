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
     * @brief Render system that draws every GameObject with a Model using the standard vertex-lit pipeline.
     *
     * Owns its own VkPipelineLayout and Pipeline (built from
     * simple_shader.vert/frag.spv using the Model::Vertex input layout).
     * Per-object model/normal matrices are pushed via vkCmdPushConstants()
     * rather than a per-object uniform buffer.
     */
    class SolidRenderSystem
    {
    public:
        /**
         * @brief Calls createPipelineLayout() then createPipeline().
         * @param device Device the pipeline layout/pipeline are created on.
         * @param renderPass Render pass the pipeline will be compatible with.
         * @param globalSetLayout Layout of the global (binding-0 uniform buffer) descriptor set, included in the pipeline layout.
         */
        SolidRenderSystem(Device &device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout);
        /// Calls vkDestroyPipelineLayout() (the Pipeline member destroys the VkPipeline itself).
        ~SolidRenderSystem();

        SolidRenderSystem (const SolidRenderSystem &) = delete;
        SolidRenderSystem& operator=(const SolidRenderSystem &) = delete;

        /// Binds the pipeline and global descriptor set, then for each GameObject with a Model pushes its model/normal matrices and calls Model::bind()/Model::draw().
        void renderGameObjects(FrameInfo &frameInfo);
    private:
        /// Calls vkCreatePipelineLayout() with globalSetLayout plus a push-constant range sized for SimplePushConstantData.
        void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
        /// Builds a default PipelineConfigInfo and constructs the Pipeline from the simple-shader vert/frag pair.
        void createPipeline(VkRenderPass renderPass);

        Device &device;
        // This lets us create a new swap chain just by creating a new swapChain object
        VkPipelineLayout pipelineLayout;
        std::unique_ptr<Pipeline> pipeline;
    };
}
