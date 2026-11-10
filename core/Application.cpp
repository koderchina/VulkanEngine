#include "Application.hpp"

#include "Camera.hpp"
#include "systems/SolidRenderSystem.hpp"
#include "systems/PointLightRenderSystem.hpp"
#include "passes/ForwardPass.hpp"
#include "passes/GBufferPass.hpp"
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

#include <csignal>

namespace Engine
{
    sig_atomic_t sigflag = 0;

    void sighandler(int s)
    {
        sigflag = 1;
    }

    Application::Application()
    {
        std::signal(SIGINT, sighandler);

        loadGameObjects();

        // Populates the texture index
        loadTextures();

        globalPool = DescriptorPool::Builder(device)
            .setMaxSets(SwapChain::MAX_FRAMES_IN_FLIGHT + static_cast<uint32_t>(textureIndex.size()))
            .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, SwapChain::MAX_FRAMES_IN_FLIGHT)
            .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, static_cast<uint32_t>(textureIndex.size()))
            .build();
    }

    Application::~Application()
    {
        
    }

    void Application::loadGameObjects()
    {
        auto sponza = GameObject::createGameObject();
        sponza.model = Model::createModelFromFile(device, "models/sponza/sponza.obj");
        sponza.transform.translation = {-0.5f, 0.0f, -1.0f};
        sponza.transform.rotation = {glm::radians(180.0f), 0.0f, 0.0f};
        sponza.transform.scale = {0.01f, 0.01f, 0.01f};
        gameObjects.emplace(sponza.getId(), std::move(sponza));

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

    void Application::loadTextures()
    {
        textureIndex[""] = Image::createSolidColor(device);

        for (auto &[id, obj] : gameObjects)
        {
            if (!obj.model)
            {
                // Game object is not a mesh, continue.
                continue;
            }
            
            for (auto &submesh : obj.model->getSubmheshes())
            {
                const std::string &path = submesh.texPath;
                if (textureIndex.count(path))
                {
                    // Texture already loaded, continue.
                    continue;
                }

                auto image = Image::createImageFromFile(device, path);
                if (image != nullptr)
                {
                    textureIndex[path] = std::move(image);
                }
                else
                {
                    textureIndex[path] = Image::createSolidColor(device);
                }
            }
        }

        std::cout << "Loaded " << textureIndex.size() << " unique textures.\n";
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

        auto materialSetLayout = DescriptorSetLayout::Builder(device)
            .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .build();

        for (auto &[path, image] : textureIndex)
        {
            VkDescriptorImageInfo imageInfo{};
            imageInfo.imageLayout = image->getLayout();
            imageInfo.imageView = image->getView();
            imageInfo.sampler = image->getSampler();

            VkDescriptorSet set;
            DescriptorWriter(*materialSetLayout, *globalPool).writeImage(0, &imageInfo).build(set);
            materialDescriptorSets[path] = set;
        }

        VkDescriptorImageInfo descriptorImageInfo{};
        descriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        // descriptorImageInfo.imageView = 

        std::vector<VkDescriptorSet> globalDescriptorSets(SwapChain::MAX_FRAMES_IN_FLIGHT);
        for (int i = 0; i < globalDescriptorSets.size(); ++i)
        {
            auto bufferInfo = uboBuffers[i]->descriptorInfo();
            DescriptorWriter(*globalSetLayout, *globalPool)
                .writeBuffer(0, &bufferInfo)
                .build(globalDescriptorSets[i]);
        }

        std::vector<VkDescriptorSetLayout> layoutDescriptions{globalSetLayout->getDescriptorSetLayout(), materialSetLayout->getDescriptorSetLayout()};

        // SolidRenderSystem solidRenderSystem{device, renderer.getColorAttachmentFormat(), renderer.getDepthAttachmentFormat(), layoutDescriptions};
        PointLightRenderSystem pointLightRenderSystem{device, renderer.getColorAttachmentFormat(), renderer.getDepthAttachmentFormat(), globalSetLayout->getDescriptorSetLayout()};
        Camera camera{};

        // This game object has no model and isn't rendered
        // It just stores the camera state
        auto viewerObject = GameObject::createGameObject();
        viewerObject.transform.translation = {-4.0f, -1.0f, -1.0f};
        viewerObject.transform.rotation = {0.0f, glm::radians(90.0f), 0.0f};
        KeyboardController cameraController{};
        MouseController cameraMouse{window.getWindowPtr()};

        // Lives for the whole loop so ForwardPass can keep a reference to it; the per-frame
        // fields are overwritten at the start of every frame.
        FrameInfo frameInfo{0, 0.0f, VK_NULL_HANDLE, camera, VK_NULL_HANDLE, gameObjects, materialDescriptorSets};

        RenderResource depth{};
        depth.name = "depth";
        // Same format the pipelines were built with (VkPipelineRenderingCreateInfo::depthAttachmentFormat).
        depth.format = renderer.getDepthAttachmentFormat();
        depth.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        depth.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        // Extent = swap chain extent, and resized together with the swap chain.
        renderer.addScreenSizedResource(depth);

        /**
         * G-Buffer definition:
         *  - albedo: texture + vertex color
         *  - normal: world-space normals in xyz
         *  - position: world-space position in xyz
         */
        RenderResource albedo{};
        albedo.name = "albedo";
        albedo.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        albedo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        albedo.format = VK_FORMAT_R8G8B8A8_SRGB;
        renderer.addScreenSizedResource(albedo);

        RenderResource normal{};
        normal.name = "normal";
        normal.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        normal.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        normal.format = VK_FORMAT_R16G16B16A16_SFLOAT;
        renderer.addScreenSizedResource(normal);

        RenderResource position{};
        position.name = "position";
        position.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        position.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        position.format = VK_FORMAT_R16G16B16A16_SFLOAT;
        renderer.addScreenSizedResource(position);

        SolidRenderSystem solidRenderSystem(
            device, 
            {albedo.format, normal.format, position.format}, 
            "shaders/gbuffer.vert.spv", 
            "shaders/gbuffer.frag.spv", 
            depth.format,
            layoutDescriptions
        );

        auto& graph = renderer.getGraph();
        graph.addPass(std::make_unique<GBufferPass>(frameInfo, solidRenderSystem));
        graph.addPass(std::make_unique<ForwardPass>(frameInfo, pointLightRenderSystem));
        // Orders the passes and allocates "depth" (vkCreateImage/vkAllocateMemory/vkCreateImageView).
        graph.compile();

        auto currentTime = std::chrono::high_resolution_clock::now();

        while(!window.shouldClose())
        {
            if (glfwGetKey(window.getWindowPtr(), GLFW_KEY_ESCAPE) == GLFW_PRESS || (sigflag != 0))
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
            // std::cout << viewerObject.transform.translation.x << " " << viewerObject.transform.translation.y << " " << viewerObject.transform.translation.z << "\n";
            camera.setViewYXZ(viewerObject.transform.translation, viewerObject.transform.rotation);

            auto commandBuffer = renderer.beginFrame();
            if (commandBuffer != nullptr)
            {
                int frameIndex = renderer.getFrameIndex();
                frameInfo.FrameIndex = frameIndex;
                frameInfo.frameTime = frameTime;
                frameInfo.commandBuffer = commandBuffer;
                frameInfo.globalDescriptorSet = globalDescriptorSets[frameIndex];

                // Update objects in memory
                GlobalUniformBufferObject uniformBufferObject{};
                uniformBufferObject.projection = camera.getProjection();
                uniformBufferObject.view = camera.getView();
                uniformBufferObject.inverseView = camera.getInverseView();
                pointLightRenderSystem.update(frameInfo, uniformBufferObject);
                uboBuffers[frameIndex]->writeToBuffer((void *)&uniformBufferObject);
                uboBuffers[frameIndex]->flush();

                // Barriers + every pass (vkCmdBeginRendering ... draws ... vkCmdEndRendering)
                renderer.executeRenderGraph(commandBuffer);
                renderer.endFrame();
            }
        }

        // The last drawFrame() submission is still in flight on the GPU when the
        // loop exits; wait for it to finish before destructors start tearing down
        // the pipeline, render pass, image views, and sync objects it references.
        vkDeviceWaitIdle(device.device());
    }
}