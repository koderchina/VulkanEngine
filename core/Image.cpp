#include "Image.hpp"

#include <iostream>
#include <stdexcept>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace Engine
{
    Image::Image(Device &device, VkImageCreateInfo imageInfo, VkMemoryPropertyFlags memoryFlags) : device{device}
    {
        device.createImageWithInfo(imageInfo, memoryFlags, image, memory);
        imageExtent = imageInfo.extent;
        imageFormat = imageInfo.format;
        currentImageLayout = imageInfo.initialLayout;
        
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.pNext = nullptr;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.anisotropyEnable = VK_FALSE; // Need to add check if device supports it
        samplerInfo.maxAnisotropy = device.properties.limits.maxSamplerAnisotropy;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        // For now
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 0.0f;

        device.createSampler(samplerInfo, imageSampler);
    }

    Image::~Image()
    {
        // Destroy sampler if one exists.
        if (imageSampler != VK_NULL_HANDLE)
        {
            vkDestroySampler(device.device(), imageSampler, nullptr);
        }
        // Destroy image view (if one is created)
        if (imageView != VK_NULL_HANDLE)
        {
            vkDestroyImageView(device.device(), imageView, nullptr);
        }
        // Destroy image
        vkDestroyImage(device.device(), image, nullptr);
        // Free device memory
        vkFreeMemory(device.device(), memory, nullptr);
    }

    void Image::transitionLayout(
        VkImageLayout newLayout, 
        VkAccessFlags srcMask, 
        VkAccessFlags dstMask,
        VkPipelineStageFlagBits srcStage,
        VkPipelineStageFlagBits dstStage
    )
    {

        VkCommandBuffer cmd = device.beginSingleTimeCommands();
        
        VkImageSubresourceRange range{};
        range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        range.baseMipLevel = 0;
        range.levelCount = 1;
        range.baseArrayLayer = 0;
        range.layerCount = 1;

        VkImageMemoryBarrier transferImageBarrier{};
        transferImageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        transferImageBarrier.oldLayout = currentImageLayout;
        transferImageBarrier.newLayout = newLayout;
        transferImageBarrier.image = image;
        transferImageBarrier.subresourceRange = range;
        transferImageBarrier.srcAccessMask = srcMask;
        transferImageBarrier.dstAccessMask = dstMask;

        vkCmdPipelineBarrier(
            cmd, 
            srcStage, 
            dstStage, 
            0, 0, nullptr, 0, nullptr, 1, 
            &transferImageBarrier
        );

        device.endSingleTimeCommands(cmd);

        currentImageLayout = newLayout;
    }

    std::unique_ptr<Image> Image::createImageFromFile(Device &device, const std::string &path)
    {
        int width, height, channels;

        stbi_uc* pixels = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);
        if (!pixels)
        {
            std::cout << "Failed to load texture file " << path << std::endl;
            return nullptr;
        }

        std::unique_ptr<Image> newImage = createFromPixelData(device, pixels, width, height);
        stbi_image_free(pixels);
        return newImage;
    }

    std::unique_ptr<Image> Image::createSolidColor(Device &device, std::array<uint8_t, 4> rgba)
    {
        return createFromPixelData(device, rgba.data(), 1, 1);
    }

    std::unique_ptr<Image> Image::createFromPixelData(Device &device, void *pixels, int width, int height)
    {
        std::unique_ptr<Image> newImage = nullptr;

        constexpr int bytesPerChannel = 4;

        VkDeviceSize imageSize = width * height * bytesPerChannel;

        Buffer stagingBuffer{
            device,
            imageSize,
            1,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        };

        stagingBuffer.map();
        stagingBuffer.writeToBuffer(pixels);

        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
        imageInfo.extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1};
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.pNext = nullptr;

        newImage = std::make_unique<Image>(device, imageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        VkImageViewCreateInfo imageViewInfo{};
        imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewInfo.pNext = nullptr;
        imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imageViewInfo.image = newImage->getImage();
        imageViewInfo.format = newImage->getFormat();
        imageViewInfo.subresourceRange.baseMipLevel = 0;
        imageViewInfo.subresourceRange.levelCount = 1;
        imageViewInfo.subresourceRange.baseArrayLayer = 0;
        imageViewInfo.subresourceRange.layerCount = 1;
        imageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

        newImage->transitionLayout(
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            0,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT);

        device.copyBufferToImage(
            stagingBuffer.getBuffer(),
            newImage->getImage(),
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height),
            1);

        newImage->transitionLayout(
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        
        if (vkCreateImageView(device.device(), &imageViewInfo, nullptr, &newImage->imageView) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create image view!");
        }

        return newImage;
    }
}