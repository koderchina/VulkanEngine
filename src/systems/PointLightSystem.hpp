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
    class PointLightSystem
    {
    public:
        PointLightSystem(Device &device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout);
        ~PointLightSystem();

        PointLightSystem (const PointLightSystem &) = delete;
        PointLightSystem& operator=(const PointLightSystem &) = delete;

        void render(FrameInfo &frameInfo);
        void update(FrameInfo &frameInfo, GlobalUniformBufferObject &ubo);
    private:
        void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
        void createPipeline(VkRenderPass renderPass);

        Device &device;
        // This lets us create a new swap chain just by creating a new swapChain object
        VkPipelineLayout pipelineLayout;
        std::unique_ptr<Pipeline> pipeline;
    };
}