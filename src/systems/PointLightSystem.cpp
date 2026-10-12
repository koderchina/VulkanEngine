#include "PointLightSystem.hpp"

#define GLM_FROCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include <map>

namespace Engine
{
    struct PointLightPushConstant
    {
        glm::vec4 position{};
        glm::vec4 color{};
        float radius;
    };

    PointLightSystem::PointLightSystem(Device &device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout) 
        : device{device}
    {
        createPipelineLayout(globalSetLayout);
        createPipeline(renderPass);
    }

    PointLightSystem::~PointLightSystem()
    {
        vkDestroyPipelineLayout(device.device(), pipelineLayout, nullptr);
    }

    void PointLightSystem::createPipelineLayout(VkDescriptorSetLayout globalSetLayout)
    {
        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(PointLightPushConstant);

        std::vector<VkDescriptorSetLayout> descriptorSetLayouts{globalSetLayout};

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
        pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

        if (vkCreatePipelineLayout(device.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create pipeline layout.");
        }
    }

    void PointLightSystem::createPipeline(VkRenderPass renderPass)
    {
        assert(pipelineLayout != nullptr && "Cannot create pipeline before pipeline layout!");

        PipelineConfigInfo pipelineConfig{};
        Pipeline::defaultPipelineConfigInfo(pipelineConfig);
        Pipeline::enableAlphaBlending(pipelineConfig);
        // Special case
        pipelineConfig.bindingDescriptions.clear();
        pipelineConfig.attributeDescriptions.clear();
        
        pipelineConfig.renderPass = renderPass;
        pipelineConfig.pipelineLayout = pipelineLayout;
        pipeline = std::make_unique<Pipeline>(
            device, 
            pipelineConfig, 
            "./shaders/point_light.vert.spv", 
            "./shaders/point_light.frag.spv"
        );
    }

    void PointLightSystem::update(FrameInfo &frameInfo, GlobalUniformBufferObject &ubo)
    {
        int lightIndex = 0;
        for (auto &kv : frameInfo.gameObjects)
        {
            auto &gameObject = kv.second;
            if (gameObject.pointLight == nullptr)
            {
                continue;
            }

            // Update point light position
            auto rotateRight = glm::rotate(glm::mat4(1.0f), 
                frameInfo.frameTime,
                {0.0f, -1.0f, 0.0f});
            // gameObject.transform.translation = glm::vec3(rotateRight * glm::vec4(gameObject.transform.translation, 1.f));

            ubo.pointLights[lightIndex].position = glm::vec4(gameObject.transform.translation, 1.f);
            ubo.pointLights[lightIndex].color = glm::vec4(gameObject.color, gameObject.pointLight->lightIntensity);

            ++lightIndex;
        }

        ubo.numLights = lightIndex;
    }

    void PointLightSystem::render(FrameInfo &frameInfo)
    {
        std::map<float, GameObject::id_t> sorted;
        for (auto &kv : frameInfo.gameObjects)
        {
            auto &obj = kv.second;
            if (obj.pointLight == nullptr)
            {
                continue;
            }

            //calculate distance
            auto offset = frameInfo.camera.getPosition() - obj.transform.translation;
            float distSquared = glm::dot(offset, offset);
            sorted[distSquared] = obj.getId();
        }
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
            nullptr
        );

        for (auto it = sorted.rbegin(); it != sorted.rend(); ++it)
        {
            auto &gameObject = frameInfo.gameObjects.at(it->second);

            PointLightPushConstant push{};
            push.position = glm::vec4(gameObject.transform.translation, 1.0f);
            push.color = glm::vec4(gameObject.color, gameObject.pointLight->lightIntensity);
            push.radius = gameObject.transform.scale.x;

            vkCmdPushConstants(frameInfo.commandBuffer, 
                pipelineLayout, 
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                0, sizeof(PointLightPushConstant),
                (void *)(&push)
            );

            vkCmdDraw(frameInfo.commandBuffer, 6, 1, 0, 0);
        }
    }
}