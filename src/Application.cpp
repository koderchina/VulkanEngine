#include "Application.hpp"

#include "Camera.hpp"
#include "systems/SimpleRenderSystem.hpp"
#include "systems/PointLightSystem.hpp"
#include "KeyboardController.hpp"
#include "MouseController.hpp"
#include "Buffer.hpp"

#define GLM_FROCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include <chrono>
#include <memory>
#include <stdexcept>
#include <array>
#include <filesystem>

namespace Engine
{
    Application::Application()
    {
        globalPool = DescriptorPool::Builder(device)
            .setMaxSets(SwapChain::MAX_FRAMES_IN_FLIGHT)
            .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, SwapChain::MAX_FRAMES_IN_FLIGHT)
            .build();
        loadGameObjects();
    }

    Application::~Application()
    {
        
    }

    void Application::loadGameObjects()
    {
        auto flatVase = GameObject::createGameObject();
        flatVase.model = Model::createModelFromFile(device, "models/flat_vase.obj");
        flatVase.transform.translation = {-0.5f, 0.5f, 0.0f};
        flatVase.transform.scale = {3.0f, 1.5f, 3.0f};
        gameObjects.emplace(flatVase.getId(), std::move(flatVase));

        auto smoothVase = GameObject::createGameObject();
        smoothVase.model = Model::createModelFromFile(device, "models/smooth_vase.obj");
        smoothVase.transform.translation = {0.5f, 0.5f, 0.0f};
        smoothVase.transform.scale = {3.0f, 1.5f, 3.0f};
        gameObjects.emplace(smoothVase.getId(), std::move(smoothVase));

        auto floor = GameObject::createGameObject();
        floor.model = Model::createModelFromFile(device, "models/quad.obj");
        floor.transform.translation = {0.0f, 0.5f, 0.0f};
        floor.transform.scale = {5.0f, 1.0f, 5.0f};
        gameObjects.emplace(floor.getId(), std::move(floor));

         std::vector<glm::vec3> lightColors
         {
            {1.f, .1f, .1f},
            {.1f, .1f, 1.f},
            {.1f, 1.f, .1f},
            {1.f, 1.f, .1f},
            {.1f, 1.f, 1.f},
            {1.f, 1.f, 1.f}  //
        };
        for (int i = 0; i < lightColors.size(); ++i)
        {
            auto pointLight = GameObject::makePointLight(0.2f);
            pointLight.color = lightColors[i];
            auto rotateRight = glm::rotate(glm::mat4(1.0f), (i * glm::two_pi<float>()) / lightColors.size(),
                {0.0f, -1.0f, 0.0f});
            pointLight.transform.translation = glm::vec3(rotateRight * glm::vec4(-1.0f, -1.0f, -1.0f, 1.0f));
            gameObjects.emplace(pointLight.getId(), std::move(pointLight));
        }

        std::cout << "Loaded " << gameObjects.size() << " game objects.\n";
    }


    void Application::run()
    {
        std::vector<std::unique_ptr<Buffer>> uboBuffers(SwapChain::MAX_FRAMES_IN_FLIGHT);
        for (int i = 0; i < uboBuffers.size(); ++i)
        {
            uboBuffers[i] = std::make_unique<Buffer>(
                device,
                sizeof(GlobalUniformBufferObject),
                1,
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                device.properties.limits.minUniformBufferOffsetAlignment
            );
            uboBuffers[i]->map();
        }

        auto globalSetLayout = DescriptorSetLayout::Builder(device)
            .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
            .build();

        std::vector<VkDescriptorSet> globalDescriptorSets(SwapChain::MAX_FRAMES_IN_FLIGHT);
        for (int i = 0; i < globalDescriptorSets.size(); ++i)
        {
            auto bufferInfo = uboBuffers[i]->descriptorInfo();
            DescriptorWriter(*globalSetLayout, *globalPool)
                .writeBuffer(0, &bufferInfo)
                .build(globalDescriptorSets[i]);
        }

        SimpleRenderSystem simpleRenderSystem{device, renderer.getRenderPass(), globalSetLayout->getDescriptorSetLayout()};
        PointLightSystem pointLightSystem{device, renderer.getRenderPass(), globalSetLayout->getDescriptorSetLayout()};
        Camera camera{};

        // This game object has no model and isn't rendered
        // It just stores the camera state
        auto viewerObject = GameObject::createGameObject();
        KeyboardController cameraController{};
        MouseController cameraMouse{window.getWindowPtr()};
        
        auto currentTime = std::chrono::high_resolution_clock::now();

        while(!window.shouldClose())
        {
            if (glfwGetKey(window.getWindowPtr(), GLFW_KEY_ESCAPE) == GLFW_PRESS)
            {
                glfwSetWindowShouldClose(window.getWindowPtr(), GLFW_TRUE);
            }
            glfwPollEvents();
            
            auto newTime = std::chrono::high_resolution_clock::now();
            float frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
            currentTime = newTime;

            float aspect = renderer.getAspectRatio();
            camera.setPerspectiveProjection(glm::radians(60.0f), aspect, 0.1f, 100.0f);
            cameraController.moveInPlaneXZ(window.getWindowPtr(), frameTime, viewerObject);
            cameraMouse.move(frameTime, viewerObject);
            camera.setViewYXZ(viewerObject.transform.translation, viewerObject.transform.rotation);

            auto commandBuffer = renderer.beginFrame();
            if (commandBuffer != nullptr)
            {
                int frameIndex = renderer.getFrameIndex();
                FrameInfo frameInfo {
                    frameIndex,
                    frameTime,
                    commandBuffer,
                    camera,
                    globalDescriptorSets[frameIndex],
                    gameObjects
                };

                // Update objects in memory
                GlobalUniformBufferObject uniformBufferObject{};
                uniformBufferObject.projection = camera.getProjection();
                uniformBufferObject.view = camera.getView();
                uniformBufferObject.inverseView = camera.getInverseView();
                pointLightSystem.update(frameInfo, uniformBufferObject);
                uboBuffers[frameIndex]->writeToBuffer((void *)&uniformBufferObject);
                uboBuffers[frameIndex]->flush();

                // render
                renderer.beginRenderPass(commandBuffer);

                // Render solid objects 
                simpleRenderSystem.renderGameObjects(frameInfo);
                // Render semi-transparent objects
                pointLightSystem.render(frameInfo);
                
                renderer.endRenderPass(commandBuffer);
                renderer.endFrame();
            }
        }

        // The last drawFrame() submission is still in flight on the GPU when the
        // loop exits; wait for it to finish before destructors start tearing down
        // the pipeline, render pass, image views, and sync objects it references.
        vkDeviceWaitIdle(device.device());
    }
}