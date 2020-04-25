#pragma once

#include <volk.h>

#include <vector>

#include "locator.h"
#include "device.h"
#include "vertex.h"
#include "command.h"

namespace create {
    void buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
        VkBufferCreateInfo bufferInfo = {};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        hw::loc::device()->create(bufferInfo, buffer);

        VkMemoryRequirements memRequirements;
        hw::loc::device()->get(buffer, memRequirements);

        VkMemoryAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = hw::loc::device()->find(memRequirements.memoryTypeBits, properties);

        hw::loc::device()->allocate(allocInfo, bufferMemory);

        hw::loc::device()->bind(buffer, bufferMemory);
    }

    VkImageView imageView(VkImage& image, VkFormat format=VK_FORMAT_R8G8B8A8_SRGB, VkImageAspectFlags aspectFlags=VK_IMAGE_ASPECT_COLOR_BIT, int layerCount=1, VkImageViewType viewType=VK_IMAGE_VIEW_TYPE_2D) {
        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image;
        viewInfo.viewType = viewType;
        viewInfo.format = format;
        viewInfo.subresourceRange.aspectMask = aspectFlags;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = layerCount;

        VkImageView imageView;
        hw::loc::device()->create(viewInfo, imageView);

        return imageView;
    }

    void sampler(VkSampler& sampler, VkSamplerAddressMode addressMode=VK_SAMPLER_ADDRESS_MODE_REPEAT) {
        VkSamplerCreateInfo samplerInfo = {};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = addressMode;
        samplerInfo.addressModeV = addressMode;
        samplerInfo.addressModeW = addressMode;
        samplerInfo.anisotropyEnable = VK_TRUE;
        samplerInfo.maxAnisotropy = 16;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

        hw::loc::device()->create(samplerInfo, sampler);
    }

    void vertexBuffer(std::vector<Vertex>& vertices, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
        VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        create::buffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

        void* data;
        hw::loc::device()->map(stagingBufferMemory, bufferSize, data);
        /**/memcpy(data, vertices.data(), (size_t) bufferSize);
        hw::loc::device()->unmap(stagingBufferMemory);

        create::buffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, buffer, bufferMemory);

        hw::loc::cmd()->copyBuffer(stagingBuffer, buffer, bufferSize);

        hw::loc::device()->destroy(stagingBuffer);
        hw::loc::device()->free(stagingBufferMemory);
    }

    void indexBuffer(std::vector<uint32_t>& indeces, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
        VkDeviceSize bufferSize = sizeof(indeces[0]) * indeces.size();

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        create::buffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

        void* data;
        hw::loc::device()->map(stagingBufferMemory, bufferSize, data);
        /**/memcpy(data, indeces.data(), (size_t) bufferSize);
        hw::loc::device()->unmap(stagingBufferMemory);

        create::buffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, buffer, bufferMemory);

        hw::loc::cmd()->copyBuffer(stagingBuffer, buffer, bufferSize);

        hw::loc::device()->destroy(stagingBuffer);
        hw::loc::device()->free(stagingBufferMemory);
    }

    void cubemap(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory) {
        VkImageCreateInfo imageInfo = {};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = width;
        imageInfo.extent.height = height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = format;
        imageInfo.tiling = tiling;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = usage;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.arrayLayers = 6;
        imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

        hw::loc::device()->create(imageInfo, image);

        VkMemoryRequirements memRequirements;
        hw::loc::device()->get(image, memRequirements);

        VkMemoryAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = hw::loc::device()->find(memRequirements.memoryTypeBits, properties);

        hw::loc::device()->allocate(allocInfo, imageMemory);

        hw::loc::device()->bind(image, imageMemory);
    }

    void image(uint32_t width, uint32_t height, VkImageUsageFlags usage, VkImage& image, VkDeviceMemory& imageMemory, VkFormat format=VK_FORMAT_R8G8B8A8_SRGB) {
        VkImageCreateInfo imageInfo = {};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = width;
        imageInfo.extent.height = height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = format;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = usage;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        hw::loc::device()->create(imageInfo, image);

        VkMemoryRequirements memRequirements;
        hw::loc::device()->get(image, memRequirements);

        VkMemoryAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = hw::loc::device()->find(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        hw::loc::device()->allocate(allocInfo, imageMemory);

        hw::loc::device()->bind(image, imageMemory);
    }
}
