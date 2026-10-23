#include "SolidRenderSystem.hpp"

#define GLM_FROCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

namespace Engine
{
    struct SimplePushConstantData
    {
        glm::mat4 modelMatrix{1.0f}; // identity matrix
        glm::mat4 normalMatrix{1.0f};
    };

    SolidRenderSystem::SolidRenderSystem(Device &device, VkRenderPass renderPass, std::vector<VkDescriptorSetLayout> &layouts) 
        : device{device}
    {
        createPipelineLayout(layouts);
        createPipeline(renderPass);
    }

    SolidRenderSystem::~SolidRenderSystem()
    {
        vkDestroyPipelineLayout(device.device(), pipelineLayout, nullptr);
    }

    void SolidRenderSystem::createPipelineLayout(std::vector<VkDescriptorSetLayout> &layouts)
    {
        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushConstantRange.size = sizeof(SimplePushConstantData);

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(layouts.size());
        pipelineLayoutInfo.pSetLayouts = layouts.data();
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

        if (vkCreatePipelineLayout(device.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create pipeline layout.");
        }
    }

    void SolidRenderSystem::createPipeline(VkRenderPass renderPass)
    {
        assert(pipelineLayout != nullptr && "Cannot create pipeline before pipeline layout!");

        PipelineConfigInfo pipelineConfig{};
        Pipeline::defaultPipelineConfigInfo(pipelineConfig);
        pipelineConfig.renderPass = renderPass;
        pipelineConfig.pipelineLayout = pipelineLayout;
        pipeline = std::make_unique<Pipeline>(
            device, 
            pipelineConfig, 
            "./shaders/simple_shader.vert.spv", 
            "./shaders/simple_shader.frag.spv"
        );
    }

    void SolidRenderSystem::renderGameObjects(FrameInfo &frameInfo)
    {
        // render
        // vkCmdBindPipeline: use this pipeline
        pipeline->bind(frameInfo.commandBuffer);

        vkCmdBindDescriptorSets(
            frameInfo.commandBuffer, 
            VK_PIPELINE_BIND_POINT_GRAPHICS, 
            pipelineLayout, 
            0, 1, 
            &frameInfo.globalDescriptorSet, 
            0, 
            nullptr);

        for (auto& kv : frameInfo.gameObjects)
        {
            auto &obj = kv.second;
            if (obj.model == nullptr)
            {
                continue;
            }

            SimplePushConstantData push{};
            push.modelMatrix = obj.transform.mat4();
            push.normalMatrix = obj.transform.normalMatrix();

            vkCmdPushConstants(
                frameInfo.commandBuffer, 
                pipelineLayout, 
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 
                0, 
                sizeof(SimplePushConstantData), 
                &push
            );

            obj.model->bind(frameInfo.commandBuffer);
            // obj.model->draw(frameInfo.commandBuffer);

            for (auto submesh : obj.model->getSubmheshes())
            {
                if (frameInfo.materialDescriptorSets.count(submesh.texPath))
                {
                    auto &set = frameInfo.materialDescriptorSets[submesh.texPath];
                    vkCmdBindDescriptorSets(
                        frameInfo.commandBuffer,
                        VK_PIPELINE_BIND_POINT_GRAPHICS,
                        pipelineLayout,
                        1, 1,
                        &set,
                        0,
                        nullptr
                    );
                }

                obj.model->drawSubmesh(frameInfo.commandBuffer, submesh);
            }
        }
    }
}