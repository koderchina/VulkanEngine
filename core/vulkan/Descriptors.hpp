#pragma once

#include "Device.hpp"

#include <memory>
#include <unordered_map>
#include <vector>

namespace Engine
{
    /**
     * @brief Owns a VkDescriptorSetLayout describing the bindings a shader expects (binding slot, type, stage, count).
     *
     * Construction fills a VkDescriptorSetLayoutCreateInfo from the collected
     * bindings and calls vkCreateDescriptorSetLayout(). DescriptorWriter reads
     * the `bindings` map back to validate writes against it.
     */
    class DescriptorSetLayout
    {
    public:
        /// Fluent builder that accumulates VkDescriptorSetLayoutBinding entries before creating a DescriptorSetLayout.
        class Builder
        {
        public:
            Builder(Device &device) : device{device} {}

            /**
             * @brief Records one binding slot (binding -> VkDescriptorSetLayoutBinding).
             * @param binding Binding number referenced from shaders/descriptor writes.
             * @param descriptorType Type of descriptor (uniform buffer, sampler, etc.).
             * @param stageFlags Shader stages that may access this binding.
             * @param count Number of descriptors in the binding (array size).
             * @return *this, for chaining.
             */
            Builder &addBinding(
                uint32_t binding,
                VkDescriptorType descriptorType,
                VkShaderStageFlags stageFlags,
                uint32_t count = 1);
            /// @return A new DescriptorSetLayout built from the accumulated bindings (calls vkCreateDescriptorSetLayout()).
            std::unique_ptr<DescriptorSetLayout> build() const;

        private:
            Device &device;
            std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings{};
        };

        /**
         * @brief Calls vkCreateDescriptorSetLayout() from an already-built bindings map.
         * @param device Device the layout is created on.
         * @param bindings Binding-number -> VkDescriptorSetLayoutBinding map, usually produced by Builder.
         */
        DescriptorSetLayout(
        Device &device, std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings);
        /// Calls vkDestroyDescriptorSetLayout().
        ~DescriptorSetLayout();
        DescriptorSetLayout(const DescriptorSetLayout &) = delete;
        DescriptorSetLayout &operator=(const DescriptorSetLayout &) = delete;

        /// @return The underlying VkDescriptorSetLayout handle.
        VkDescriptorSetLayout getDescriptorSetLayout() const { return descriptorSetLayout; }

    private:
        Device &device;
        VkDescriptorSetLayout descriptorSetLayout;
        std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings;

        friend class DescriptorWriter;
    };

    /**
     * @brief Owns a VkDescriptorPool that descriptor sets are allocated out of.
     *
     * Construction calls vkCreateDescriptorPool() sized for `maxSets` sets and
     * the given per-type VkDescriptorPoolSize budget.
     */
    class DescriptorPool
    {
    public:
        /// Fluent builder that accumulates pool sizes/flags/maxSets before creating a DescriptorPool.
        class Builder
        {
        public:
            Builder(Device &device) : device{device} {}

            /**
             * @brief Adds a VkDescriptorPoolSize entry (how many descriptors of a given type the pool can hand out).
             * @param descriptorType Descriptor type this budget applies to.
             * @param count Number of descriptors of that type the pool should support.
             * @return *this, for chaining.
             */
            Builder &addPoolSize(VkDescriptorType descriptorType, uint32_t count);
            /**
             * @brief Sets VkDescriptorPoolCreateInfo::flags (e.g. VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT).
             * @param flags Pool creation flags.
             * @return *this, for chaining.
             */
            Builder &setPoolFlags(VkDescriptorPoolCreateFlags flags);
            /**
             * @brief Sets the maximum number of descriptor sets the pool can allocate.
             * @param count Max set count.
             * @return *this, for chaining.
             */
            Builder &setMaxSets(uint32_t count);

            /// @return A new DescriptorPool built from the accumulated sizes/flags (calls vkCreateDescriptorPool()).
            std::unique_ptr<DescriptorPool> build() const;
        private:
            Device &device;
            std::vector<VkDescriptorPoolSize> poolSizes{};
            uint32_t maxSets = 1000;
            VkDescriptorPoolCreateFlags poolFlags = 0;
        };

        /**
         * @brief Calls vkCreateDescriptorPool().
         * @param device Device the pool is created on.
         * @param maxSets Maximum number of descriptor sets allocatable from the pool.
         * @param poolFlags VkDescriptorPoolCreateFlags.
         * @param poolSizes Per-descriptor-type budget.
         */
        DescriptorPool(Device &device, uint32_t maxSets, VkDescriptorPoolCreateFlags poolFlags, const std::vector<VkDescriptorPoolSize> &poolSizes);
        /// Calls vkDestroyDescriptorPool() (which also frees every set allocated from it).
        ~DescriptorPool();

        DescriptorPool(const DescriptorPool &) = delete;
        DescriptorPool &operator=(const DescriptorPool &) = delete;

        /**
         * @brief Calls vkAllocateDescriptorSets() for a single set of the given layout.
         * @param descriptorSetLayout Layout the new set must match.
         * @param descriptor Out parameter receiving the allocated VkDescriptorSet.
         * @return true on VK_SUCCESS, false otherwise (e.g. pool exhausted).
         */
        bool allocateDescriptor(const VkDescriptorSetLayout descriptorSetLayout, VkDescriptorSet &descriptor) const;

        /**
         * @brief Calls vkFreeDescriptorSets() to return sets to the pool.
         * @param descriptorSets Sets to free; requires the pool to have been created with VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT.
         */
        void freeDescriptors(std::vector<VkDescriptorSet> &descriptorSets) const;

        /// Calls vkResetDescriptorPool(), implicitly freeing every set ever allocated from it.
        void resetPool();

    private:
        Device &device;
        VkDescriptorPool descriptorPool;

        friend class DescriptorWriter;
    };

    /**
     * @brief Builds up a batch of VkWriteDescriptorSet entries and applies them with a single vkUpdateDescriptorSets() call.
     *
     * Validates each binding against the DescriptorSetLayout it is given before
     * queueing the write, so a binding/type mismatch asserts immediately.
     */
    class DescriptorWriter
    {
    public:
        /**
         * @brief Binds a layout (for validating writes) and a pool (for allocating sets in build()).
         * @param setLayout Layout describing the bindings being written.
         * @param pool Pool used by build() to allocate the destination set.
         */
        DescriptorWriter(DescriptorSetLayout &setLayout, DescriptorPool &pool);

        /**
         * @brief Queues a VkWriteDescriptorSet for a buffer-backed binding (e.g. uniform/storage buffer).
         * @param binding Binding number, must exist in setLayout.
         * @param bufferInfo Buffer range to bind, usually from Buffer::descriptorInfo().
         * @return *this, for chaining.
         */
        DescriptorWriter &writeBuffer(uint32_t binding, VkDescriptorBufferInfo *bufferInfo);
        /**
         * @brief Queues a VkWriteDescriptorSet for an image/sampler binding.
         * @param binding Binding number, must exist in setLayout.
         * @param imageInfo Image view/sampler/layout to bind.
         * @return *this, for chaining.
         */
        DescriptorWriter &writeImage(uint32_t binding, VkDescriptorImageInfo *imageInfo);

        /**
         * @brief Allocates a new set from the pool (vkAllocateDescriptorSets()) then writes the queued entries into it.
         * @param set Out parameter receiving the allocated and written set.
         * @return false if allocation failed, true otherwise.
         */
        bool build(VkDescriptorSet &set);
        /**
         * @brief Writes the queued entries into an already-allocated set via vkUpdateDescriptorSets().
         * @param set Existing descriptor set to update.
         */
        void overwrite(VkDescriptorSet &set);

    private:
        DescriptorSetLayout &setLayout;
        DescriptorPool &pool;
        std::vector<VkWriteDescriptorSet> writes;
    };
}
