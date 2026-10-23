#pragma once

#include "Window.hpp"

#include <VkBootstrap.h>

// std lib headers
#include <string>
#include <vector>

namespace Engine
{

/**
 * @brief Owns the VkInstance, VkPhysicalDevice, VkDevice and the queues/command pool built from them.
 *
 * vk-bootstrap (vkb::*) is used to do the verbose parts of instance/device
 * creation (layer & extension enumeration, physical device scoring, queue
 * family selection) and hands back the plain Vulkan handles this class
 * stores and exposes to the rest of the engine. Also provides the buffer/
 * image allocation helpers (vkCreateBuffer + vkAllocateMemory, etc.) that
 * every other Vulkan object in the engine is built on top of.
 */
class Device
{
public:
#ifdef NDEBUG
  const bool enableValidationLayers = false;
#else
  const bool enableValidationLayers = true;
#endif

  /**
   * @brief Builds the instance, surface, physical/logical device and command pool, in that order.
   * @param window Window used to query the required WSI extensions and to create the VkSurfaceKHR.
   */
  Device(Window &window);

  /// Destroys the command pool, logical device, debug messenger, surface and instance (reverse creation order).
  ~Device();

  // Not copyable or movable
  Device(const Device &) = delete;
  void operator=(const Device &) = delete;
  Device(Device &&) = delete;
  Device &operator=(Device &&) = delete;

  /// @return The VkCommandPool that all command buffers in the engine are allocated from.
  VkCommandPool getCommandPool() { return commandPool; }
  /// @return The logical VkDevice handle (vkCreateDevice result).
  VkDevice device() { return device_; }
  /// @return The VkSurfaceKHR created from the window.
  VkSurfaceKHR surface() { return surface_; }
  /// @return The VkQueue used for vkQueueSubmit of graphics/compute command buffers.
  VkQueue graphicsQueue() { return graphicsQueue_; }
  /// @return The VkQueue used for vkQueuePresentKHR.
  VkQueue presentQueue() { return presentQueue_; }
  // vk-bootstrap's view of the device (queue families, surface, physical device).
  // SwapchainBuilder needs it to pick the image sharing mode.
  /// @return vk-bootstrap's device info (queue families, surface, physical device) needed by vkb::SwapchainBuilder.
  const vkb::Device &vkbDevice() const { return vkbDevice_; }

  /**
   * @brief Picks a memory type index satisfying both a type filter and required property flags.
   *
   * Wraps vkGetPhysicalDeviceMemoryProperties() and scans memoryTypes for the first
   * bit set in typeFilter whose propertyFlags also contain `properties`.
   * @param typeFilter Bitmask from VkMemoryRequirements::memoryTypeBits.
   * @param properties Required VkMemoryPropertyFlags (e.g. host-visible, device-local).
   * @return Index into VkPhysicalDeviceMemoryProperties::memoryTypes.
   */
  uint32_t findMemoryType(uint32_t typeFilter,
                          VkMemoryPropertyFlags properties);
  /**
   * @brief Finds the first candidate VkFormat supported for the given tiling and feature flags.
   *
   * Calls vkGetPhysicalDeviceFormatProperties() for each candidate and checks
   * linearTilingFeatures or optimalTilingFeatures depending on `tiling`.
   * @param candidates Formats to try, in preference order.
   * @param tiling VK_IMAGE_TILING_LINEAR or VK_IMAGE_TILING_OPTIMAL.
   * @param features Required VkFormatFeatureFlags (e.g. depth/stencil attachment support).
   * @return The first matching format.
   */
  VkFormat findSupportedFormat(const std::vector<VkFormat> &candidates,
                               VkImageTiling tiling,
                               VkFormatFeatureFlags features);

  // Buffer Helper Functions
  /**
   * @brief Creates a VkBuffer and backs it with newly allocated, bound device memory.
   *
   * Calls vkCreateBuffer(), then vkGetBufferMemoryRequirements(),
   * vkAllocateMemory() (via findMemoryType()) and vkBindBufferMemory().
   * @param size Buffer size in bytes.
   * @param usage VkBufferUsageFlags (vertex/index/uniform/transfer, etc.).
   * @param properties Required VkMemoryPropertyFlags for the backing allocation.
   * @param buffer Out parameter receiving the created VkBuffer.
   * @param bufferMemory Out parameter receiving the bound VkDeviceMemory.
   */
  void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                    VkMemoryPropertyFlags properties, VkBuffer &buffer,
                    VkDeviceMemory &bufferMemory);
  /**
   * @brief Allocates and begins a one-shot primary command buffer.
   *
   * vkAllocateCommandBuffers() from commandPool followed by
   * vkBeginCommandBuffer() with VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT.
   * Pair with endSingleTimeCommands().
   * @return The recording-ready command buffer.
   */
  VkCommandBuffer beginSingleTimeCommands();
  /**
   * @brief Ends, submits and synchronously waits on a command buffer started with beginSingleTimeCommands().
   *
   * vkEndCommandBuffer(), then vkQueueSubmit() on the graphics queue,
   * vkQueueWaitIdle() to block until it finishes, and vkFreeCommandBuffers().
   * @param commandBuffer The command buffer previously returned by beginSingleTimeCommands().
   */
  void endSingleTimeCommands(VkCommandBuffer commandBuffer);
  /**
   * @brief Records and submits a vkCmdCopyBuffer() between two buffers.
   * @param srcBuffer Source buffer handle.
   * @param dstBuffer Destination buffer handle.
   * @param size Number of bytes to copy.
   */
  void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
  /**
   * @brief Records and submits a vkCmdCopyBufferToImage() into a single 2D image layer.
   * @param buffer Source buffer (e.g. a staging buffer).
   * @param image Destination image, expected to already be in VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL.
   * @param width Image copy width in texels.
   * @param height Image copy height in texels.
   * @param layerCount Number of array layers to copy.
   */
  void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width,
                         uint32_t height, uint32_t layerCount);

  /**
   * @brief Creates a VkImage and backs it with newly allocated, bound device memory.
   *
   * Calls vkCreateImage(), then vkGetImageMemoryRequirements(),
   * vkAllocateMemory() (via findMemoryType()) and vkBindImageMemory().
   * @param imageInfo Fully filled-in VkImageCreateInfo describing the image.
   * @param properties Required VkMemoryPropertyFlags for the backing allocation.
   * @param image Out parameter receiving the created VkImage.
   * @param imageMemory Out parameter receiving the bound VkDeviceMemory.
   */
  void createImageWithInfo(const VkImageCreateInfo &imageInfo,
                           VkMemoryPropertyFlags properties, VkImage &image,
                           VkDeviceMemory &imageMemory);
  
  void createSampler(const VkSamplerCreateInfo samplerInfo, VkSampler &sampler);

  /// Physical device properties (vkGetPhysicalDeviceProperties), e.g. limits.minUniformBufferOffsetAlignment.
  VkPhysicalDeviceProperties properties;

private:
  // vkCreateInstance + vkCreateDebugUtilsMessengerEXT (vkb::InstanceBuilder)
  /// Builds the VkInstance (and, in debug builds, the debug messenger) via vkb::InstanceBuilder.
  vkb::Instance createInstance();
  /// Calls window.createWindowSurface() to obtain surface_.
  void createSurface();
  // vkEnumeratePhysicalDevices + suitability checks (vkb::PhysicalDeviceSelector)
  /// Scores and selects a VkPhysicalDevice via vkb::PhysicalDeviceSelector.
  vkb::PhysicalDevice pickPhysicalDevice(const vkb::Instance &vkbInstance);
  // vkCreateDevice + vkGetDeviceQueue (vkb::DeviceBuilder)
  /// Builds the logical VkDevice and fetches the graphics/present VkQueues via vkb::DeviceBuilder.
  void createLogicalDevice(const vkb::PhysicalDevice &vkbPhysicalDevice);
  /// Calls vkCreateCommandPool() on the graphics queue family.
  void createCommandPool();

  VkInstance instance;
  VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
  VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
  vkb::Device vkbDevice_;
  Window &window;
  /***
   * A command pool manages the physical memory for command buffers.
   * When a command buffer is created the memory it uses is taken from the command pool.
   * Recording commands (vkCmdDraw, vkCmdBindPipeline, etc.) is essentially writing an
   * implementation-defined instruction stream into the host memory (the GPU doesn't read it
   * directly at record time). On submission (vkQueueSubmit), the driver is responsible for getting
   * that command stream to the GPU in whatever form the hardware actually consumes.
   */
  VkCommandPool commandPool;

  VkDevice device_;
  VkSurfaceKHR surface_;
  VkQueue graphicsQueue_;
  VkQueue presentQueue_;
  uint32_t graphicsQueueFamily;
};

} // namespace Engine
