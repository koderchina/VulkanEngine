#pragma once

#include "Camera.hpp"
#include "GameObject.hpp"

#include <vulkan/vulkan.h>

#define MAX_LIGHTS 10

namespace Engine
{
    struct PointLight
    {
        glm::vec4 position{}; // ignore w
        glm::vec4 color{}; // w is intensity
    };

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