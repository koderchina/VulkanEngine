#pragma once
#include "Device.hpp"
#include "Pipeline.hpp"
#include "Descriptors.hpp"
#include "FrameInfo.hpp"

#include <memory>
#include <string>

namespace Engine
{
    class LightingSystem
    {
    public:
        LightingSystem(
            Device& device, 
            VkFormat colorFormat, 
            const DescriptorSetLayout& globalSetLayout,
            const std::string& vertexShaderPath,
            const std::string& fragmentShaderPath
        );
        ~LightingSystem();

        LightingSystem(const LightingSystem &) = delete;
        LightingSystem &operator=(const LightingSystem &) = delete;

        void updateGBuffer(
            VkImageView albedoView,
            VkImageView normalView,
            VkImageView positionView
        );

        void render(FrameInfo& frameInfo);
    private:
        void createPipelineLayout(const DescriptorSetLayout& globalSetLayout);
        void createPipeline(
            VkFormat colorFormat,
            const std::string& vertexShaderPath,
            const std::string& fragmentShaderPath
        );
        void createSampler();

        Device& device;
        
        std::unique_ptr<DescriptorSetLayout> gBufferSetLayout;
        std::unique_ptr<DescriptorPool> gBufferSetPool; // own pool
        VkDescriptorSet gBufferSet = VK_NULL_HANDLE;

        VkSampler sampler = VK_NULL_HANDLE;
        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        std::unique_ptr<Pipeline> pipeline;
    };
}