#pragma once

#include <volk.h>

#include <stdexcept>

#include "locator.h"
#include "device.h"
#include "create.h"

namespace hw {
    class Command {
        public:
            Command(uint32_t flags=0) {
                hw::QueueFamilyIndices queueFamilyIndices = hw::loc::device()->findQueueFamilies();

                VkCommandPoolCreateInfo poolInfo = {};
                poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
                poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
                if (flags != 0)
                    poolInfo.flags = flags;

                hw::loc::device()->create(poolInfo, commandPool);
            }

            ~Command() {
                hw::loc::device()->destroy(commandPool);
            }

            VkCommandBuffer& get(uint32_t index) {
                return commandBuffers[index];
            }

            void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, int layerCount=1) {
                VkCommandBuffer commandBuffer = beginSingleTimeCommands();

                VkImageMemoryBarrier barrier = {};
                barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                barrier.oldLayout = oldLayout;
                barrier.newLayout = newLayout;
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.image = image;
                barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                barrier.subresourceRange.baseMipLevel = 0;
                barrier.subresourceRange.levelCount = 1;
                barrier.subresourceRange.baseArrayLayer = 0;
                barrier.subresourceRange.layerCount = layerCount;

                VkPipelineStageFlags sourceStage;
                VkPipelineStageFlags destinationStage;

                if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
                    barrier.srcAccessMask = 0;
                    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

                    sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                    destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
                } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
                    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

                    sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
                    destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
                } else {
                    throw std::invalid_argument("unsupported layout transition!");
                }

                vkCmdPipelineBarrier(
                        commandBuffer,
                        sourceStage, destinationStage,
                        0,
                        0, nullptr,
                        0, nullptr,
                        1, &barrier
                        );

                endSingleTimeCommands(commandBuffer);
            }

            void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, int layerCount=1) {
                VkCommandBuffer commandBuffer = beginSingleTimeCommands();

                VkBufferImageCopy region = {};
                region.bufferOffset = 0;
                region.bufferRowLength = 0;
                region.bufferImageHeight = 0;
                region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                region.imageSubresource.mipLevel = 0;
                region.imageSubresource.baseArrayLayer = 0;
                region.imageSubresource.layerCount = layerCount;
                region.imageOffset = {0, 0, 0};
                region.imageExtent = {
                    width,
                    height,
                    1
                };

                vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

                endSingleTimeCommands(commandBuffer);
            }

            void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
                VkCommandBuffer commandBuffer = beginSingleTimeCommands();

                VkBufferCopy copyRegion = {};
                copyRegion.size = size;
                vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

                endSingleTimeCommands(commandBuffer);
            }


            void createCommandBuffers(uint32_t size) {
                commandBuffers.resize(size);

                VkCommandBufferAllocateInfo allocInfo = {};
                allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
                allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
                allocInfo.commandPool = commandPool;
                allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

                hw::loc::device()->allocate(allocInfo, commandBuffers.data());
            }

            void freeCommandBuffers() {
                hw::loc::device()->free(commandPool, static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());
            }

            void customSingleCommand(bool func(VkCommandBuffer)) {
                VkCommandBuffer command_buffer = beginSingleTimeCommands();
                func(command_buffer);
                endSingleTimeCommands(command_buffer);
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

        private:
            VkCommandBuffer beginSingleTimeCommands() {
                VkCommandBufferAllocateInfo allocInfo = {};
                allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
                allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
                allocInfo.commandPool = commandPool;
                allocInfo.commandBufferCount = 1;

                VkCommandBuffer commandBuffer;
                hw::loc::device()->allocate(allocInfo, &commandBuffer);

                VkCommandBufferBeginInfo beginInfo = {};
                beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
                beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

                vkBeginCommandBuffer(commandBuffer, &beginInfo);

                return commandBuffer;
            }

            void endSingleTimeCommands(VkCommandBuffer commandBuffer) {
                vkEndCommandBuffer(commandBuffer);

                VkSubmitInfo submitInfo = {};
                submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
                submitInfo.commandBufferCount = 1;
                submitInfo.pCommandBuffers = &commandBuffer;

                hw::loc::device()->submit(submitInfo, VK_NULL_HANDLE);
                hw::loc::device()->waitQueue();

                hw::loc::device()->free(commandPool, 1, &commandBuffer);
            }

            VkCommandPool commandPool;
            std::vector<VkCommandBuffer> commandBuffers;
    };
}
