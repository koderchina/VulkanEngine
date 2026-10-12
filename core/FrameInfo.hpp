#pragma once

#include "Camera.hpp"
#include "GameObject.hpp"

#include <vulkan/vulkan.h>

#define MAX_LIGHTS 10

namespace Engine
{
    /// Single point light entry inside GlobalUniformBufferObject's fixed-size light array.
    struct PointLight
    {
        glm::vec4 position{}; // ignore w
        glm::vec4 color{}; // w is intensity
    };

    /**
     * @brief Layout of the per-frame uniform buffer bound at descriptor set binding 0.
     *
     * One instance of this struct is memcpy'd (via Buffer::writeToBuffer()) into
     * the mapped uniform buffer each frame; its field order and alignment must
     * match the `GlobalUbo` struct declared in the GLSL shaders exactly.
     */
    struct GlobalUniformBufferObject
    {
        glm::mat4 projection{1.0f};
        glm::mat4 view{1.0f};
        // Position of the camera can be extracted from the last column.
        glm::mat4 inverseView{1.0f};
        glm::vec4 ambientColor{1.0f, 1.0f, 1.0f, 0.02f}; // w is intensity
        PointLight pointLights[MAX_LIGHTS] = {};
        int numLights;
    };

    /**
     * @brief Bundle of everything a render system needs to record its draw calls for the current frame.
     *
     * Constructed once per frame in Application::run() and passed by reference
     * into SolidRenderSystem::renderGameObjects() / PointLightRenderSystem::render()/update().
     */
    struct FrameInfo
    {
        int FrameIndex;
        float frameTime;
        VkCommandBuffer commandBuffer;
        Camera &camera;
        VkDescriptorSet globalDescriptorSet;
        GameObject::Map &gameObjects;

    };
}
