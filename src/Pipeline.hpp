#pragma once

#include "Device.hpp"

#include <string>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace Engine
{
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
        VkRenderPass                            renderPass;
        uint32_t                                subpass = 0;

        std::vector<VkDynamicState> dynamicStateEnables;
        VkPipelineDynamicStateCreateInfo dynamicStateInfo;
    };

    class Pipeline
    {
    public:
        Pipeline(
            Device &device, 
            const PipelineConfigInfo &configInfo, 
            const std::string &vertShaderPath, 
            const std::string &fragShaderPath);
        
        ~Pipeline();

        Pipeline (const Pipeline&) = delete;
        Pipeline& operator=(const Pipeline&) = delete;
        
        // This method binds the command buffer object to the pipeline so it can be executed on the GPU.
        void bind(VkCommandBuffer commandBuffer);
        static void defaultPipelineConfigInfo(PipelineConfigInfo &configInfo);
        static void enableAlphaBlending(PipelineConfigInfo &configInfo);
    private:
        static std::vector<char> readFile(const std::string &filePath);
        
        void createGraphicsPipeline(
            const PipelineConfigInfo &configInfo, 
            const std::string &vertShaderPath, 
            const std::string &fragShaderPath);

        void createShaderModule(const std::vector<char> &code, VkShaderModule *shaderModule);

        Device &device;
        VkPipeline graphicsPipeline;
        VkShaderModule vertShaderModule;
        VkShaderModule fragShaderModule;
    };
}