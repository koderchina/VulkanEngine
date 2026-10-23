#include "Utils.hpp"
#include "Model.hpp"

#include <cassert>
#include <cstring>
#include <iostream>
#include <unordered_map>
#include <map>
#include <algorithm>
#include <filesystem>

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

namespace std
{
    template<>
    struct hash<Engine::Model::Vertex> 
    {
        size_t operator()(Engine::Model::Vertex const &vertex) const
        {
            size_t seed = 0;
            Engine::hashCombine(seed, vertex.position, vertex.color, vertex.normal, vertex.uv);
            return seed;
        }
    };
}
namespace Engine
{
    Model::Model(Device &device, const Model::Builder &vertices)
        : device{device}, submeshes{vertices.submeshes}
    {
        createVertexBuffers(vertices.vertices);
        createIndexBuffers(vertices.indices);
    }

    Model::~Model()
    {

    }

    std::unique_ptr<Model> Model::createModelFromFile(Device &device, const std::string &path)
    {
        Builder builder{};
        builder.loadModel(path);
        std::cout << "Vertex count: " << builder.vertices.size() << std::endl;
        return std::make_unique<Model>(device, builder);
    }

    void Model::createVertexBuffers(const std::vector<Vertex> &vertices)
    {
        vertexCount = static_cast<uint32_t>(vertices.size());
        assert(vertexCount >= 3 && "Vertex count must be at least 3!");

        VkDeviceSize bufferSize = sizeof(vertices[0]) * vertexCount;

        uint32_t vertexSize = sizeof(vertices[0]);

        /***
         * Staging buffer is stack variable so it will be destroyed when function returns.
         * VK_MEMORY_PROPERTY_HOST_COHERENT_BIT tells Vulkan to flush the staging buffer
         * contents to the GPU whenever it is changed (every time a new model is loaded).
         */
        Buffer stagingBuffer {
            device,
            vertexSize,
            vertexCount,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        };

        stagingBuffer.map();
        stagingBuffer.writeToBuffer((void *)vertices.data());

        vertexBuffer = std::make_unique<Buffer>(
            device,
            vertexSize,
            vertexCount,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT // This is GPU VRAM.
        );

        // Sends a command to the GPU to copy the contents of the staing buffer
        // to the device local memory that will actually hold the vertex.
        device.copyBuffer(stagingBuffer.getBuffer(), vertexBuffer->getBuffer(), bufferSize);
    }

    void Model::createIndexBuffers(const std::vector<uint32_t> &indices)
    {
        indexCount = static_cast<uint32_t>(indices.size());
        if (indexCount > 0)
        {
            hasIndexBuffer = true;
            uint32_t indexSize = sizeof(indices[0]);
            VkDeviceSize bufferSize = sizeof(indices[0]) * indexCount;

            Buffer stagingBuffer{
                device,
                indexSize,
                indexCount,
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
            };

            stagingBuffer.map();
            stagingBuffer.writeToBuffer((void *)indices.data());

            indexBuffer = std::make_unique<Buffer>(
                device,
                indexSize,
                indexCount,
                VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
            );

            // Sends a command to the GPU to copy the contents of the staing buffer
            // to the device local memory that will actually hold the vertex.
            device.copyBuffer(stagingBuffer.getBuffer(), indexBuffer->getBuffer(), bufferSize);
        }
        else
        {
            return;
        }
    }

    void Model::drawSubmesh(VkCommandBuffer commandBuffer, Submesh &submesh)
    {
        if (hasIndexBuffer)
        {
            vkCmdDrawIndexed(commandBuffer, submesh.indexCount, 1, submesh.firstIndex, 0, 0);
        }
    }

    /***
     * When the command is executed, primitives are assembled using the current primitive topology and
     * vertexCount consecutive vertex indices with the first vertexIndex value equal to firstVertex. 
     * The primitives are drawn instanceCount times with instanceIndex starting with firstInstance
     * and increasing sequentially for each instance. 
     * The assembled primitives execute the bound graphics pipeline.
     */
    void Model::draw(VkCommandBuffer commandBuffer)
    {
        if (hasIndexBuffer)
        {
            vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);
        }
        else
        {
            vkCmdDraw(commandBuffer, vertexCount, 1, 0, 0);
        }
    }

    void Model::bind(VkCommandBuffer commandBuffer)
    {
        VkBuffer buffers[] = {vertexBuffer->getBuffer()};
        VkDeviceSize offsets[] = {0};
        // Bind vertex buffers to a command buffer
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);

        if (hasIndexBuffer)
        {
            vkCmdBindIndexBuffer(commandBuffer, indexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
        }
    }

    std::vector<VkVertexInputBindingDescription> Model::Vertex::getBindingDescriptions()
    {
        std::vector<VkVertexInputBindingDescription> bindingDescriptions(1);
        bindingDescriptions[0].binding = 0;
        bindingDescriptions[0].stride = sizeof(Model::Vertex);
        bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return bindingDescriptions;
    }

    std::vector<VkVertexInputAttributeDescription> Model::Vertex::getAttributeDescriptions()
    {
        std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};

        attributeDescriptions.push_back({0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position)});
        attributeDescriptions.push_back({1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, color)});
        attributeDescriptions.push_back({2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal)});
        attributeDescriptions.push_back({3, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv)});

        return attributeDescriptions;
    }

    void Model::Builder::loadModel(const std::string &path)
    {
        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;

        std::string warn;
        std::string err;

        std::string baseDir = std::filesystem::path(path).parent_path().string() + "/";
        if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.c_str(), baseDir.c_str()))
        {
            throw std::runtime_error(warn + err);
        }

        vertices.clear();
        indices.clear();
        submeshes.clear();

        std::unordered_map<Vertex, uint32_t> uniqueVertices{};

        // This map binds materialId to the index subvector of the loaded mesh.
        std::map<int, std::vector<uint32_t>> indicesByMaterial{};

        for (const auto& shape : shapes)
        {
            for (size_t f = 0; f < shape.mesh.material_ids.size(); ++f)
            {
                int materialId = shape.mesh.material_ids[f];
                for (int v = 0; v < 3; ++v)
                {
                    const auto &index = shape.mesh.indices[3 * f + v];
                    Vertex vertex{};
                    if(index.vertex_index >= 0)
                    {
                        vertex.position = {
                            attrib.vertices[3 * index.vertex_index + 0],
                            attrib.vertices[3 * index.vertex_index + 1],
                            attrib.vertices[3 * index.vertex_index + 2],
                        };

                        vertex.color = {
                            attrib.colors[3 * index.vertex_index + 0],
                            attrib.colors[3 * index.vertex_index + 1],
                            attrib.colors[3 * index.vertex_index + 2],
                        };
                    }

                    if(index.normal_index >= 0)
                    {
                        vertex.normal = {
                            attrib.normals[3 * index.normal_index + 0],
                            attrib.normals[3 * index.normal_index + 1],
                            attrib.normals[3 * index.normal_index + 2],
                        };
                    }

                    if(index.texcoord_index >= 0)
                    {
                        vertex.uv = {
                            attrib.texcoords[2 * index.texcoord_index + 0],
                            attrib.texcoords[2 * index.texcoord_index + 1]
                        };
                    }

                    if(uniqueVertices.count(vertex) == 0)
                    {
                        uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
                        vertices.push_back(vertex);
                    }
                    indicesByMaterial[materialId].push_back(uniqueVertices[vertex]);
                }
            }
        }

        // Flatten the per-material buckets into one index buffer, recording
        // a Submesh range per bucket. `materialId`/`matIndices` here are new
        // names bound to each map entry's key/value - unrelated to the
        // `materialId` declared inside the loop above, whose scope already ended.
        for (auto &[materialId, matIndices] : indicesByMaterial)
        {
            Submesh submesh{};
            submesh.firstIndex = static_cast<uint32_t>(indices.size());
            submesh.indexCount = static_cast<uint32_t>(matIndices.size());

            if (materialId >= 0 && !materials[materialId].diffuse_texname.empty())
            {
                std::string texName = materials[materialId].diffuse_texname;
                std::replace(texName.begin(), texName.end(), '\\', '/');
                submesh.texPath = baseDir + texName;
            }

            indices.insert(indices.end(), matIndices.begin(), matIndices.end());
            submeshes.push_back(submesh);
        }

        std::cout << "Loaded " << vertices.size() << " vertices, "
                   << indices.size() << " indices, "
                   << materials.size() << " materials ("
                   << submeshes.size() << " submeshes) from " << path << std::endl;
    }
}