#pragma once

#include "Device.hpp"

#include <string>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace Engine
{
    /**
     * @brief Plain-data bundle of all the fixed-function pipeline state structs Pipeline::createGraphicsPipeline() plugs into VkGraphicsPipelineCreateInfo.
     *
     * Not copyable because several of its VkPipeline*CreateInfo members hold
     * pointers into this struct's own vectors (e.g. colorBlendInfo.pAttachments
     * points at colorBlendAttachment); copying would leave those pointers
     * dangling or aliased. Fill one via Pipeline::defaultPipelineConfigInfo()
     * and then override individual fields as needed.
     */
    struct PipelineConfigInfo
    {
        PipelineConfigInfo() = default;
        PipelineConfigInfo(const PipelineConfigInfo&) = delete;
        PipelineConfigInfo& operator=(const PipelineConfigInfo&) = delete;

        std::vector<VkVertexInputBindingDescription> bindingDescriptions{};
        std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};
        /*
         Combines the viewport and scissor into a single object.
         The viewport controls where the rendered image gets positioned/scaled
         within the target framebuffer, while the scissor hard-clips any pixels
         outside its rectangle. This struct just bundles the two together
         (plus how many of each, since a pipeline can use multiple with
         multi-viewport rendering) so it can be plugged into the pipeline.
        */
        VkPipelineViewportStateCreateInfo viewportInfo;
        /*
         Takes in a list of vertices and converts them into geometry.
         We don't have to be passing just the vertex coordinates, but other data
         that will be processed in the vertex shader.
        */
        VkPipelineInputAssemblyStateCreateInfo  inputAssemblyInfo;
        /*
         Breaks up geometry into fragments for each px the geometry overlaps.
        */
        VkPipelineRasterizationStateCreateInfo  rasterizationInfo;
        VkPipelineMultisampleStateCreateInfo    multisampleInfo;
        /*
         Controls how the colors are combined in the framebuffer.
         If triangles are aligned, fragment shader returns multiple color
         values for the common pixels. This pipeline stage handles that.
        */
        VkPipelineColorBlendAttachmentState     colorBlendAttachment;
        VkPipelineColorBlendStateCreateInfo     colorBlendInfo;
        /*
         Depth buffer attached to the frame buffer.
         It stores the depth value of the closes fragment for every pixel.
        */
        VkPipelineDepthStencilStateCreateInfo   depthStencilInfo;

        VkPipelineLayout                        pipelineLayout;

        /**
         * Dynamic rendering deprecates renderpasses.
         */
        VkFormat colorAttachmentFormat{VK_FORMAT_UNDEFINED};
        VkPipelineRenderingCreateInfoKHR renderingInfo{};

        std::vector<VkDynamicState> dynamicStateEnables;
        VkPipelineDynamicStateCreateInfo dynamicStateInfo;
    };

    /**
     * @brief Owns a VkPipeline (plus the two VkShaderModules it was built from) - the compiled, fixed-function-configured graphics pipeline.
     *
     * Construction reads SPIR-V bytecode from disk, wraps each stage in a
     * VkShaderModule, and calls vkCreateGraphicsPipelines() with the stage
     * info plus everything in a PipelineConfigInfo.
     */
    class Pipeline
    {
    public:
        /**
         * @brief Loads vert/frag SPIR-V, creates shader modules, and calls vkCreateGraphicsPipelines().
         * @param device Device the pipeline is created on.
         * @param configInfo Fixed-function state (viewport, rasterization, blending, layout, render pass, etc.).
         * @param vertShaderPath Path to the compiled vertex shader (.spv).
         * @param fragShaderPath Path to the compiled fragment shader (.spv).
         */
        Pipeline(
            Device &device,
            const PipelineConfigInfo &configInfo,
            const std::string &vertShaderPath,
            const std::string &fragShaderPath);

        /// Calls vkDestroyShaderModule() for both stages and vkDestroyPipeline().
        ~Pipeline();

        Pipeline (const Pipeline&) = delete;
        Pipeline& operator=(const Pipeline&) = delete;

        // This method binds the command buffer object to the pipeline so it can be executed on the GPU.
        /// Calls vkCmdBindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS) so subsequent draws in commandBuffer use this pipeline.
        void bind(VkCommandBuffer commandBuffer);
        /// Fills configInfo with the engine's standard fixed-function state (triangle list, no culling, depth test on, dynamic viewport/scissor, alpha blending off).
        static void defaultPipelineConfigInfo(PipelineConfigInfo &configInfo, VkFormat colorFormat, VkFormat depthFormat);
        /// Switches configInfo's color blend attachment to standard alpha blending (src-alpha, one-minus-src-alpha).
        static void enableAlphaBlending(PipelineConfigInfo &configInfo);
    private:
        /// Reads the whole file at filePath into a byte buffer (used to load compiled SPIR-V).
        static std::vector<char> readFile(const std::string &filePath);

        /// Loads the shader bytecode, creates shader modules, and calls vkCreateGraphicsPipelines() with the full pipeline state.
        void createGraphicsPipeline(
            const PipelineConfigInfo &configInfo,
            const std::string &vertShaderPath,
            const std::string &fragShaderPath);

        /// Calls vkCreateShaderModule() to wrap raw SPIR-V bytecode into a VkShaderModule.
        void createShaderModule(const std::vector<char> &code, VkShaderModule *shaderModule);

        Device &device;
        VkPipeline graphicsPipeline;
        VkShaderModule vertShaderModule;
        VkShaderModule fragShaderModule;
    };
}
