#pragma once

#include "Device.hpp"
#include "Buffer.hpp"

#include <vector>
#include <memory>

#define GLM_FROCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

namespace Engine
{
    // This class will take vertex data from file and allocate memory and copy the data to the GPU
    /**
     * @brief Owns the vertex/index Buffer pair for one mesh and records the vkCmdBind...() / vkCmdDraw...() calls to draw it.
     *
     * Loading goes through a staging buffer: data is memcpy'd into a
     * host-visible Buffer, then Device::copyBuffer() records a
     * vkCmdCopyBuffer() to move it into device-local memory that the GPU
     * actually reads from during drawing.
     */
    class Model
    {
    public:

        /// Per-vertex attributes matching the shader's `layout(location = N) in` declarations.
        struct Vertex
        {
            glm::vec3 position{};
            glm::vec3 color{};
            glm::vec3 normal{};
            glm::vec2 uv{};

            /// @return A single VkVertexInputBindingDescription describing the stride/rate of this Vertex struct.
            static std::vector<VkVertexInputBindingDescription> getBindingDescriptions();
            /// @return One VkVertexInputAttributeDescription per field (position, color, normal, uv), with their byte offsets.
            static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();

            bool operator==(const Vertex &other) const
            {
                return position == other.position
                    && color == other.color
                    && normal == other.normal
                    && uv == other.uv;
            }
        };

        /***
         * A texture is owned by a material which binds to a contignous range of indices within a model.
         * Subset of mesh indices bound to a certain material (texture) is described by a separate descriptor set.
         */
        struct Submesh
        {
            uint32_t firstIndex;
            uint32_t indexCount;
            std::string texPath{};
        };

        /// Accumulates deduplicated vertices/indices for a Model, populated by loadModel().
        struct Builder
        {
            std::vector<Vertex> vertices{};
            std::vector<uint32_t> indices{};
            std::vector<Submesh> submeshes{};

            /**
             * @brief Parses an .obj file with tinyobjloader and deduplicates vertices (via a hash map) into vertices/indices.
             * @param path Path to the .obj file to load.
             */
            void loadModel(const std::string &path);
        };

        /**
         * @brief Calls createVertexBuffers() and createIndexBuffers() to upload the mesh data to the GPU.
         * @param device Device the vertex/index buffers are created on.
         * @param vertices Vertex and index data to upload.
         */
        Model(Device &device, const Model::Builder &vertices);
        ~Model();

        Model (const Model &) = delete;
        Model& operator=(const Model &) = delete;

        /**
         * @brief Loads an .obj file via Builder::loadModel() and constructs a Model from it.
         * @param device Device the model's buffers are created on.
         * @param path Path to the .obj file.
         * @return A newly constructed Model, ready to bind()/draw().
         */
        static std::unique_ptr<Model> createModelFromFile(Device &device, const std::string &path);
        
        const std::vector<Submesh>& getSubmheshes() {return submeshes;}
        void drawSubmesh(VkCommandBuffer commandBuffer, Submesh &submesh);

        /// Calls vkCmdBindVertexBuffers() (and vkCmdBindIndexBuffer() if indexed) on commandBuffer.
        void bind(VkCommandBuffer commandBuffer);
        /// Calls vkCmdDrawIndexed() if the model has an index buffer, otherwise vkCmdDraw().
        void draw(VkCommandBuffer commandBuffer);
    private:

        /// Builds a host-visible staging Buffer, memcpy's vertices into it, then Device::copyBuffer()s into a device-local vertexBuffer.
        void createVertexBuffers(const std::vector<Vertex> &vertices);
        /// Same staging-buffer pattern as createVertexBuffers(), for the index buffer; no-op if indices is empty.
        void createIndexBuffers(const std::vector<uint32_t> &indices);

        Device &device;

        std::unique_ptr<Buffer> vertexBuffer;
        uint32_t vertexCount;

        std::unique_ptr<Buffer> indexBuffer;
        uint32_t indexCount;

        std::vector<Submesh> submeshes;
        bool hasIndexBuffer = false;
    };
}
