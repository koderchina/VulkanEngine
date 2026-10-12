#pragma once

#include "Device.hpp"

namespace Engine
{

    /**
     * @brief Owns a VkBuffer + VkDeviceMemory pair and the host-mapping/flush logic around it.
     *
     * Construction calls Device::createBuffer() (vkCreateBuffer + vkAllocateMemory +
     * vkBindBufferMemory). The instance/alignment bookkeeping lets a single buffer
     * hold an array of `instanceCount` elements, each padded up to the device's
     * minimum offset alignment, so it can double as a dynamic/indexed uniform buffer.
     */
    class Buffer
    {
    public:
        /**
         * @brief Allocates a VkBuffer sized for instanceCount instances of instanceSize bytes, each padded to minOffsetAlignment.
         * @param device Device the buffer and memory are created on.
         * @param instanceSize Size in bytes of a single instance.
         * @param instanceCount Number of instances the buffer holds.
         * @param usageFlags VkBufferUsageFlags (vertex/index/uniform/transfer, etc.).
         * @param memoryPropertyFlags VkMemoryPropertyFlags for the backing allocation.
         * @param minOffsetAlignment Minimum required alignment for per-instance offsets (e.g. minUniformBufferOffsetAlignment).
         */
        Buffer(
            Device& device,
            VkDeviceSize instanceSize,
            uint32_t instanceCount,
            VkBufferUsageFlags usageFlags,
            VkMemoryPropertyFlags memoryPropertyFlags,
            VkDeviceSize minOffsetAlignment = 1);
        /// Unmaps (if mapped), then vkDestroyBuffer()s the buffer and vkFreeMemory()s its backing allocation.
        ~Buffer();

        Buffer(const Buffer&) = delete;
        Buffer& operator=(const Buffer&) = delete;

        /**
         * @brief Calls vkMapMemory() to map a range of the buffer's memory into host address space.
         * @param size Range to map, or VK_WHOLE_SIZE for the whole buffer.
         * @param offset Byte offset into the memory to start mapping from.
         * @return VkResult of vkMapMemory().
         */
        VkResult map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
        /// Calls vkUnmapMemory() if currently mapped.
        void unmap();

        /**
         * @brief memcpy()s data into the already-mapped host pointer.
         * @param data Source data to copy from.
         * @param size Number of bytes to copy, or VK_WHOLE_SIZE for the whole buffer.
         * @param offset Byte offset into the mapped region to copy to.
         */
        void writeToBuffer(void* data, VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
        /**
         * @brief Calls vkFlushMappedMemoryRanges() so host writes become visible to the device.
         * @note Only needed for non-coherent memory types.
         * @param size Range to flush, or VK_WHOLE_SIZE for the whole buffer.
         * @param offset Byte offset of the range to flush.
         * @return VkResult of vkFlushMappedMemoryRanges().
         */
        VkResult flush(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
        /**
         * @brief Builds a VkDescriptorBufferInfo pointing at a range of this buffer, for use in a descriptor write.
         * @param size Range described, or VK_WHOLE_SIZE for the whole buffer.
         * @param offset Byte offset of the range.
         * @return A VkDescriptorBufferInfo{buffer, offset, size}.
         */
        VkDescriptorBufferInfo descriptorInfo(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
        /**
         * @brief Calls vkInvalidateMappedMemoryRanges() so device writes become visible to the host.
         * @note Only needed for non-coherent memory types.
         * @param size Range to invalidate, or VK_WHOLE_SIZE for the whole buffer.
         * @param offset Byte offset of the range to invalidate.
         * @return VkResult of vkInvalidateMappedMemoryRanges().
         */
        VkResult invalidate(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);

        /**
         * @brief writeToBuffer() at the offset of the given instance index (index * alignmentSize).
         * @param data Source data to copy from, instanceSize bytes.
         * @param index Index of the instance to write.
         */
        void writeToIndex(void* data, int index);
        /**
         * @brief flush() the memory range of a single instance at the given index.
         * @param index Index of the instance to flush.
         * @return VkResult of vkFlushMappedMemoryRanges().
         */
        VkResult flushIndex(int index);
        /**
         * @brief descriptorInfo() scoped to a single instance at the given index.
         * @param index Index of the instance.
         * @return A VkDescriptorBufferInfo for that instance's range.
         */
        VkDescriptorBufferInfo descriptorInfoForIndex(int index);
        /**
         * @brief invalidate() the memory range of a single instance at the given index.
         * @param index Index of the instance.
         * @return VkResult of vkInvalidateMappedMemoryRanges().
         */
        VkResult invalidateIndex(int index);

        /// @return The underlying VkBuffer handle.
        VkBuffer getBuffer() const { return buffer; }
        /// @return The host pointer from the last successful map(), or nullptr if unmapped.
        void* getMappedMemory() const { return mapped; }
        /// @return Number of instances the buffer was sized for.
        uint32_t getInstanceCount() const { return instanceCount; }
        /// @return Size in bytes of a single (unpadded) instance.
        VkDeviceSize getInstanceSize() const { return instanceSize; }
        /// @return Per-instance stride after alignment padding.
        VkDeviceSize getAlignmentSize() const { return instanceSize; }
        /// @return The VkBufferUsageFlags the buffer was created with.
        VkBufferUsageFlags getUsageFlags() const { return usageFlags; }
        /// @return The VkMemoryPropertyFlags the backing memory was allocated with.
        VkMemoryPropertyFlags getMemoryPropertyFlags() const { return memoryPropertyFlags; }
        /// @return Total buffer size in bytes (alignmentSize * instanceCount).
        VkDeviceSize getBufferSize() const { return bufferSize; }

    private:
    /**
     * @brief Rounds instanceSize up to the nearest multiple of minOffsetAlignment.
     * @param instanceSize Size of a single instance.
     * @param minOffsetAlignment Required alignment in bytes (e.g. minUniformBufferOffsetAlignment); 0 disables padding.
     * @return The padded per-instance stride.
     */
    static VkDeviceSize getAlignment(VkDeviceSize instanceSize, VkDeviceSize minOffsetAlignment);

    Device& device;
    void* mapped = nullptr;
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;

    VkDeviceSize bufferSize;
    uint32_t instanceCount;
    VkDeviceSize instanceSize;
    VkDeviceSize alignmentSize;
    VkBufferUsageFlags usageFlags;
    VkMemoryPropertyFlags memoryPropertyFlags;
    };

}  // namespace lve
