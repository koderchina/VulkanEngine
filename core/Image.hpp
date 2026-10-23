#pragma once

#include "Device.hpp"
#include "Buffer.hpp"

#include <array>
#include <memory>

namespace Engine
{
    constexpr int bytesPerPixel = 4;

    class Image
    {
    public:
        Image(Device &device, VkImageCreateInfo imageInfo, VkMemoryPropertyFlags memoryFlags);
        ~Image();

        Image(const Image&) = delete;
        Image& operator=(const Image&) = delete;

        static std::unique_ptr<Image> createImageFromFile(Device &device, const std::string &path);
        static std::unique_ptr<Image> createSolidColor(Device &device, std::array<uint8_t, 4> rgba = {255, 255, 255, 255});

        VkImage getImage() {return image;}
        VkImageView getView() {return imageView;}
        VkImageLayout getLayout() {return currentImageLayout;}
        VkFormat getFormat() {return imageFormat;}
        VkSampler getSampler() {return imageSampler;}
    
    private:
        void transitionLayout(
            VkImageLayout newLayout, 
            VkAccessFlags srcMask, 
            VkAccessFlags dstMask,
            VkPipelineStageFlagBits srcStage,
            VkPipelineStageFlagBits dstStage
        );

        static std::unique_ptr<Image> createFromPixelData(Device &device, void *pixels, int width, int height);

        int imageWidth = 0;
        int imageHeight = 0;
        Device& device;
        VkImage image {VK_NULL_HANDLE};
        VkImageView imageView {VK_NULL_HANDLE};
        VkDeviceMemory memory {VK_NULL_HANDLE};
        VkFormat imageFormat{};
        VkImageLayout currentImageLayout {VK_IMAGE_LAYOUT_UNDEFINED};
        VkExtent3D imageExtent{};
        VkSampler imageSampler{VK_NULL_HANDLE};
    };
}