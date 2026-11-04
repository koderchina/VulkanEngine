#include "Device.hpp"

// std headers
#include <iostream>
#include <stdexcept>

namespace Engine
{

// local callback functions
static VKAPI_ATTR VkBool32 VKAPI_CALL
debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
              VkDebugUtilsMessageTypeFlagsEXT messageType,
              const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
              void *pUserData)
{
  std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

  return VK_FALSE;
}

// class member functions
Device::Device(Window &window) : window{window}
{
  vkb::Instance vkbInstance = createInstance();
  createSurface();
  vkb::PhysicalDevice vkbPhysicalDevice = pickPhysicalDevice(vkbInstance);
  createLogicalDevice(vkbPhysicalDevice);
  createCommandPool();
}

Device::~Device()
{
  vkDestroyCommandPool(device_, commandPool, nullptr);
  vkDestroyDevice(device_, nullptr);

  if (debugMessenger != VK_NULL_HANDLE)
  {
    // Wraps the vkGetInstanceProcAddr("vkDestroyDebugUtilsMessengerEXT") lookup
    vkb::destroy_debug_utils_messenger(instance, debugMessenger);
  }

  vkDestroySurfaceKHR(instance, surface_, nullptr);
  vkDestroyInstance(instance, nullptr);
}

/*
 vkb::InstanceBuilder fills VkApplicationInfo + VkInstanceCreateInfo and calls vkCreateInstance.
 Along the way it:
   - checks VK_LAYER_KHRONOS_validation is available (vkEnumerateInstanceLayerProperties)
   - adds the window-system extensions (VK_KHR_surface + the platform one, the same list
     glfwGetRequiredInstanceExtensions returns) and VK_EXT_debug_utils, and checks they exist
     (vkEnumerateInstanceExtensionProperties)
   - chains a VkDebugUtilsMessengerCreateInfoEXT into pNext so vkCreateInstance/vkDestroyInstance
     themselves are validated, then creates the real messenger with vkCreateDebugUtilsMessengerEXT
 */
vkb::Instance Device::createInstance()
{
  vkb::InstanceBuilder builder;
  builder.set_app_name("Engine App")
      .set_app_version(VK_MAKE_VERSION(1, 0, 0))
      .set_engine_name("No Engine")
      .set_engine_version(VK_MAKE_VERSION(1, 0, 0))
      .require_api_version(VK_API_VERSION_1_3)
      .enable_validation_layers(enableValidationLayers);

  /*
   The validation layers print debug messages to the standard output by default.
   By creating a debug messenger we can handle debug messages using callbacks.
   */
  if (enableValidationLayers)
  {
    builder.set_debug_callback(debugCallback)
        .set_debug_messenger_severity(VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                      VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        .set_debug_messenger_type(VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                  VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                  VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT);
  }

  auto result = builder.build();
  if (!result)
  {
    throw std::runtime_error("failed to create instance: " + result.error().message());
  }

  vkb::Instance vkbInstance = result.value();
  instance = vkbInstance.instance;
  debugMessenger = vkbInstance.debug_messenger;

  VkResult res = vkEnumerateInstanceVersion(&vkVersion);
  if (res == VK_SUCCESS)
  {
    std::cout << "Vulkan v." << 
      VK_API_VERSION_MAJOR(vkVersion) << "." << 
      VK_API_VERSION_MINOR(vkVersion) << "." << 
      VK_API_VERSION_PATCH(vkVersion) 
      << "\n";
  }
  return vkbInstance;
}

/*
 After initializing the Vulkan library through VkInstance we need to look for a GPU that supports the
 features we need for rendering.
 The GPU description will be stored inside a VkPhysicalDevice object that will be destroyed automatically
 when VkInstance is destroyed.

 vkb::PhysicalDeviceSelector calls vkEnumeratePhysicalDevices and rejects any GPU that:
   - has no queue family with VK_QUEUE_GRAPHICS_BIT
   - has no queue family that can present to surface_ (vkGetPhysicalDeviceSurfaceSupportKHR)
   - lacks VK_KHR_swapchain, or reports no surface formats / present modes
   - doesn't support the required features (vkGetPhysicalDeviceFeatures)
 Of the GPUs left it prefers a discrete one.
 */
vkb::PhysicalDevice Device::pickPhysicalDevice(const vkb::Instance &vkbInstance)
{
  VkPhysicalDeviceFeatures requiredFeatures{};
  requiredFeatures.samplerAnisotropy = VK_TRUE;

  VkPhysicalDeviceVulkan13Features requiredFeatures13{};
  requiredFeatures13.dynamicRendering = VK_TRUE;

  vkb::PhysicalDeviceSelector selector{vkbInstance};
  auto result = selector.set_surface(surface_)
                    .set_required_features(requiredFeatures)
                    .set_required_features_13(requiredFeatures13)
                    .select();
  if (!result)
  {
    throw std::runtime_error("failed to find a suitable GPU: " + result.error().message());
  }

  vkb::PhysicalDevice vkbPhysicalDevice = result.value();
  physicalDevice = vkbPhysicalDevice.physical_device;
  properties = vkbPhysicalDevice.properties;
  std::cout << "physical device: " << properties.deviceName << std::endl;
  return vkbPhysicalDevice;
}

/*
 Logical device describes the features of the physical device we want to use during runtime.

 vkb::DeviceBuilder builds one VkDeviceQueueCreateInfo per queue family, enables VK_KHR_swapchain
 and the features required in pickPhysicalDevice (samplerAnisotropy), and calls vkCreateDevice.
 get_queue() is vkGetDeviceQueue on the family it picked for that role.
 */
void Device::createLogicalDevice(const vkb::PhysicalDevice &vkbPhysicalDevice)
{
  vkb::DeviceBuilder builder{vkbPhysicalDevice};
  auto result = builder.build();
  if (!result)
  {
    throw std::runtime_error("failed to create logical device: " + result.error().message());
  }

  vkbDevice_ = result.value();
  device_ = vkbDevice_.device;

  auto graphicsQueue = vkbDevice_.get_queue(vkb::QueueType::graphics);
  auto presentQueue = vkbDevice_.get_queue(vkb::QueueType::present);
  if (!graphicsQueue || !presentQueue)
  {
    throw std::runtime_error("failed to get graphics/present queues!");
  }
  graphicsQueue_ = graphicsQueue.value();
  presentQueue_ = presentQueue.value();
  graphicsQueueFamily = vkbDevice_.get_queue_index(vkb::QueueType::graphics).value();
}

void Device::createCommandPool()
{
  VkCommandPoolCreateInfo poolInfo = {};
  poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  poolInfo.queueFamilyIndex = graphicsQueueFamily;
  poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                   VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

  if (vkCreateCommandPool(device_, &poolInfo, nullptr, &commandPool) !=
      VK_SUCCESS)
  {
    throw std::runtime_error("failed to create command pool!");
  }
}

void Device::createSurface()
{
  window.createWindowSurface(instance, &surface_);
}

VkFormat Device::findSupportedFormat(const std::vector<VkFormat> &candidates,
                                     VkImageTiling tiling,
                                     VkFormatFeatureFlags features) 
{
  for (VkFormat format : candidates) 
  {
    VkFormatProperties props;
    vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

    if (tiling == VK_IMAGE_TILING_LINEAR &&
        (props.linearTilingFeatures & features) == features) 
    {
      return format;
    } else if (tiling == VK_IMAGE_TILING_OPTIMAL &&
               (props.optimalTilingFeatures & features) == features) 
    {
      return format;
    }
  }
  throw std::runtime_error("failed to find supported format!");
}

uint32_t Device::findMemoryType(uint32_t typeFilter,
                                VkMemoryPropertyFlags properties) 
{
  VkPhysicalDeviceMemoryProperties memProperties;
  vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
  for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) 
  {
    if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags &
                                    properties) == properties) 
    {
      return i;
    }
  }

  throw std::runtime_error("failed to find suitable memory type!");
}

void Device::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                          VkMemoryPropertyFlags properties, VkBuffer &buffer,
                          VkDeviceMemory &bufferMemory) 
{
  VkBufferCreateInfo bufferInfo{};
  bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferInfo.size = size;
  bufferInfo.usage = usage;
  bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  if (vkCreateBuffer(device_, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) 
  {
    throw std::runtime_error("failed to create vertex buffer!");
  }

  VkMemoryRequirements memRequirements;
  vkGetBufferMemoryRequirements(device_, buffer, &memRequirements);

  VkMemoryAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.allocationSize = memRequirements.size;
  allocInfo.memoryTypeIndex =
      findMemoryType(memRequirements.memoryTypeBits, properties);

  if (vkAllocateMemory(device_, &allocInfo, nullptr, &bufferMemory) !=
      VK_SUCCESS) 
  {
    throw std::runtime_error("failed to allocate vertex buffer memory!");
  }

  vkBindBufferMemory(device_, buffer, bufferMemory, 0);
}

VkCommandBuffer Device::beginSingleTimeCommands() 
{
  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandPool = commandPool;
  allocInfo.commandBufferCount = 1;

  VkCommandBuffer commandBuffer;
  vkAllocateCommandBuffers(device_, &allocInfo, &commandBuffer);

  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  vkBeginCommandBuffer(commandBuffer, &beginInfo);
  return commandBuffer;
}

void Device::endSingleTimeCommands(VkCommandBuffer commandBuffer) 
{
  vkEndCommandBuffer(commandBuffer);

  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;

  vkQueueSubmit(graphicsQueue_, 1, &submitInfo, VK_NULL_HANDLE);
  vkQueueWaitIdle(graphicsQueue_);

  vkFreeCommandBuffers(device_, commandPool, 1, &commandBuffer);
}

void Device::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer,
                        VkDeviceSize size) 
{
  VkCommandBuffer commandBuffer = beginSingleTimeCommands();

  VkBufferCopy copyRegion{};
  copyRegion.srcOffset = 0; // Optional
  copyRegion.dstOffset = 0; // Optional
  copyRegion.size = size;
  vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

  endSingleTimeCommands(commandBuffer);
}

void Device::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width,
                               uint32_t height, uint32_t layerCount) 
{
  VkCommandBuffer commandBuffer = beginSingleTimeCommands();

  VkBufferImageCopy region{};
  region.bufferOffset = 0;
  region.bufferRowLength = 0;
  region.bufferImageHeight = 0;

  region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  region.imageSubresource.mipLevel = 0;
  region.imageSubresource.baseArrayLayer = 0;
  region.imageSubresource.layerCount = layerCount;

  region.imageOffset = {0, 0, 0};
  region.imageExtent = {width, height, 1};

  vkCmdCopyBufferToImage(commandBuffer, buffer, image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
  endSingleTimeCommands(commandBuffer);
}

void Device::createImageWithInfo(const VkImageCreateInfo &imageInfo,
                                 VkMemoryPropertyFlags properties,
                                 VkImage &image, VkDeviceMemory &imageMemory) 
{
  if (vkCreateImage(device_, &imageInfo, nullptr, &image) != VK_SUCCESS) 
  {
    throw std::runtime_error("failed to create image!");
  }

  VkMemoryRequirements memRequirements;
  vkGetImageMemoryRequirements(device_, image, &memRequirements);

  VkMemoryAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.allocationSize = memRequirements.size;
  allocInfo.memoryTypeIndex =
      findMemoryType(memRequirements.memoryTypeBits, properties);

  if (vkAllocateMemory(device_, &allocInfo, nullptr, &imageMemory) !=
      VK_SUCCESS) 
  {
    throw std::runtime_error("failed to allocate image memory!");
  }

  if (vkBindImageMemory(device_, image, imageMemory, 0) != VK_SUCCESS) 
  {
    throw std::runtime_error("failed to bind image memory!");
  }
}

void Device::createSampler(const VkSamplerCreateInfo samplerInfo, VkSampler &sampler)
{
  if (vkCreateSampler(device_, &samplerInfo, nullptr, &sampler))
  {
    throw std::runtime_error("Failed to create image sampler!");
  }
}

VkSemaphore Device::createSemaphore(const VkSemaphoreCreateInfo semInfo)
{
  VkSemaphore semaphore;
  if (vkCreateSemaphore(device_, &semInfo, nullptr, &semaphore) != VK_SUCCESS)
  {
    throw std::runtime_error("Failed to create semaphore!");
  }

  return semaphore;
}

VkImageView Device::createImageViewWithInfo(VkImageViewCreateInfo viewInfo)
{
  VkImageView imageView;
  if (vkCreateImageView(device_, &viewInfo, nullptr, &imageView) != VK_SUCCESS)
  {
    throw std::runtime_error("Failed to create image view!");
  }

  return imageView;
}

} // namespace Engine