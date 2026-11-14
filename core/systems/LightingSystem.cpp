#include "LightingSystem.hpp"

#include <stdexcept>
#include <cassert>

namespace Engine
{
    LightingSystem::LightingSystem(
        Device& device, 
        VkFormat colorFormat,
        const DescriptorSetLayout& globalSetLayout,
        const std::string& vertexShaderPath,
        const std::string& fragmentShaderPath
    ) : device{device}
    {
        gBufferSetLayout = DescriptorSetLayout::Builder(device)
            .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .build();
        
        gBufferSetPool = DescriptorPool::Builder(device)
            .setMaxSets(1)
            .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3)
            .build();

        createPipelineLayout(globalSetLayout);
        createPipeline(
            colorFormat,
            vertexShaderPath,
            fragmentShaderPath
        );

        createSampler();
    }

    LightingSystem::~LightingSystem()
    {
        vkDestroySampler(device.device(), sampler, nullptr);
        vkDestroyPipelineLayout(device.device(), pipelineLayout, nullptr);
    }

    void LightingSystem::createPipelineLayout(const DescriptorSetLayout& globalSetLayout)
    {
        VkDescriptorSetLayout layouts[] = {
            globalSetLayout.getDescriptorSetLayout(),
            gBufferSetLayout->getDescriptorSetLayout()
        };

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(sizeof(layouts) / sizeof(VkDescriptorSetLayout));
        pipelineLayoutInfo.pSetLayouts = layouts;
        pipelineLayoutInfo.pushConstantRangeCount = 0;
        pipelineLayoutInfo.pPushConstantRanges = nullptr;

        if (vkCreatePipelineLayout(device.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create pipeline layout.");
        }
    }

    void LightingSystem::createPipeline(
        VkFormat colorFormat,
        const std::string& vertexShaderPath,
        const std::string& fragmentShaderPath
    )
    {
        assert(pipelineLayout != VK_NULL_HANDLE && "Cannot create pipeline before pipeline layout!");

        PipelineConfigInfo pipelineConfig{};
        Pipeline::defaultPipelineConfigInfo(pipelineConfig, {colorFormat}, VK_FORMAT_UNDEFINED);
        pipelineConfig.pipelineLayout = pipelineLayout;
        pipelineConfig.bindingDescriptions.clear();
        pipelineConfig.attributeDescriptions.clear();
        pipelineConfig.depthStencilInfo.depthTestEnable = VK_FALSE;
        pipelineConfig.depthStencilInfo.depthWriteEnable = VK_FALSE;

        pipeline = std::make_unique<Pipeline>(
            device,
            pipelineConfig, 
            vertexShaderPath,
            fragmentShaderPath
        );
    }

    void LightingSystem::createSampler()
    {
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.minFilter = VK_FILTER_NEAREST;
        samplerInfo.magFilter = VK_FILTER_NEAREST;
        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

        if (vkCreateSampler(device.device(), &samplerInfo, nullptr, &sampler) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create image sampler.");
        }
    }

    void LightingSystem::updateGBuffer(
        VkImageView albedoView,
        VkImageView normalView,
        VkImageView positionView
    )
    {
        VkImageView views[3] = {albedoView, normalView, positionView};
        VkDescriptorImageInfo infos[3] = {};
        for (int i = 0; i < 3; ++i)
        {
            infos[i].imageView = views[i];
            infos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            infos[i].sampler = sampler;
        }

        DescriptorWriter writer(*gBufferSetLayout, *gBufferSetPool);
        writer.writeImage(0, &infos[0])
            .writeImage(1, &infos[1])
            .writeImage(2, &infos[2]);

        if (gBufferSet == VK_NULL_HANDLE)
        {
            if (!writer.build(gBufferSet))
            {
                throw std::runtime_error("Failed to allocate G-Buffer descriptor set.");
            }
        }
        else
        {
            writer.overwrite(gBufferSet);
        }

    }

    void LightingSystem::render(FrameInfo& frameInfo)
    {
        assert(gBufferSet != VK_NULL_HANDLE && "updateGBuffer() must be called before render()");

        pipeline->bind(frameInfo.commandBuffer);

        VkDescriptorSet sets[] = {frameInfo.globalDescriptorSet, gBufferSet};
        vkCmdBindDescriptorSets(
            frameInfo.commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipelineLayout,
            0, 2, sets, 0, nullptr
        );

        vkCmdDraw(frameInfo.commandBuffer, 3, 1, 0, 0);
    }
}