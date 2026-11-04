#pragma once

#include "Device.hpp"
#include "RenderResource.hpp"
#include "RenderPass.hpp"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Engine
{
    enum NodeStatus
    {
        UNVISITED,
        VISITING,
        VISITED
    };

    class RenderGraph
    {
    public:
        explicit RenderGraph(Device &device);
        /// Frees the images, views and memory of every resource the graph allocated itself.
        ~RenderGraph();

        void addResource(RenderResource resource);
        /// Takes ownership of the pass; passes touching the same resource run in the order they were added.
        void addPass(std::unique_ptr<RenderPass> renderPass);
        void compile();
        void execute(VkCommandBuffer& commandBuffer, std::unordered_map<std::string, RenderResource>& frameResources);

        /// Frees and reallocates each named resource at its new extent (e.g. after SwapChain recreation); resources not named here are left untouched.
        void resizeResources(const std::unordered_map<std::string, VkExtent3D> &newExtents);

        RenderResource* getResource(const std::string& name)
        {
            auto it = resources.find(name);
            if (it != resources.end())
            {
                return &it->second;
            }
            else
            {
                return nullptr;
            }
        }

    private:

        void visitNode(uint32_t node, 
            std::vector<std::vector<uint32_t>> &dependencies,
            std::vector<NodeStatus> &status, 
            std::vector<uint32_t> &order
        );

        void allocateResources();

        /// Looks the name up in frameResources first, then in the graph-owned resources; throws if neither has it.
        RenderResource& findResource(const std::string& name,
                                     std::unordered_map<std::string, RenderResource>& frameResources);
        /// Records a vkCmdPipelineBarrier moving `resource` from currentLayout to newLayout, then updates currentLayout.
        void transitionImage(VkCommandBuffer commandBuffer, RenderResource& resource, VkImageLayout newLayout);

        std::unordered_map<std::string, RenderResource> resources;
        // unique_ptr: RenderPass is a base class, storing it by value would slice off the subclass.
        std::vector<std::unique_ptr<RenderPass>> renderPasses;
        std::vector<uint32_t> executionOrder;

        Device& device;
    };
}