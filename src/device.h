#pragma once

#include <vulkan/vulkan.h>

#include <vector>
#include <set>

#include "insloc.h"
#include "surloc.h"

namespace hw {
    const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;

        bool isComplete() {
            return graphicsFamily.has_value() && presentFamily.has_value();
        }
    };

    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    class Device {
        public:
            Device(bool _evl) : enableValidationLayers(_evl) {
                uint32_t deviceCount = 0;
                vkEnumeratePhysicalDevices(InsLoc::instance()->get(), &deviceCount, nullptr);

                if (deviceCount == 0) {
                    throw std::runtime_error("failed to find GPUs with Vulkan support!");
                }

                std::vector<VkPhysicalDevice> devices(deviceCount);
                vkEnumeratePhysicalDevices(InsLoc::instance()->get(), &deviceCount, devices.data());

                for (const auto& _device : devices) {
                    if (isDeviceSuitable(_device)) {
                        physicalDevice = _device;
                        break;
                    }
                }

                if (physicalDevice == VK_NULL_HANDLE) {
                    throw std::runtime_error("failed to find a suitable GPU!");
                }

                QueueFamilyIndices indices;
                indices = findQueueFamilies(physicalDevice);

                VkPhysicalDeviceProperties info;
                vkGetPhysicalDeviceProperties(physicalDevice, &info);

                std::cout << info.deviceName << std::endl;

                std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
                std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};

                float queuePriority = 1.0f;
                for (uint32_t queueFamily : uniqueQueueFamilies) {
                    VkDeviceQueueCreateInfo queueCreateInfo = {};
                    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                    queueCreateInfo.queueFamilyIndex = queueFamily;
                    queueCreateInfo.queueCount = 1;
                    queueCreateInfo.pQueuePriorities = &queuePriority;
                    queueCreateInfos.push_back(queueCreateInfo);
                }

                VkPhysicalDeviceFeatures deviceFeatures = {};
                deviceFeatures.samplerAnisotropy = VK_TRUE;

                VkDeviceCreateInfo createInfo = {};
                createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

                createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
                createInfo.pQueueCreateInfos = queueCreateInfos.data();

                createInfo.pEnabledFeatures = &deviceFeatures;

                createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
                createInfo.ppEnabledExtensionNames = deviceExtensions.data();

                if (enableValidationLayers) {
                    createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
                    createInfo.ppEnabledLayerNames = validationLayers.data();
                } else {
                    createInfo.enabledLayerCount = 0;
                }

                if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
                    throw std::runtime_error("failed to create logical device!");
                }

                vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
                vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
            }

            ~Device() {
                vkDestroyDevice(device, nullptr);
            }

            VkPhysicalDevice& getPhysical() {
                return physicalDevice;
            }

            VkDevice& getLogical() {
                return device;
            }

            VkQueue& getGraphicsQueue() {
                return graphicsQueue;
            }

            void get(VkSwapchainKHR& swapchain, uint32_t& imageCount, VkImage* data) {
                vkGetSwapchainImagesKHR(device, swapchain, &imageCount, data);
            }

            void get(VkImage& image, VkMemoryRequirements& memRequirements) {
                vkGetImageMemoryRequirements(device, image, &memRequirements);
            }

            void get(VkBuffer& buffer, VkMemoryRequirements& memRequirements) {
                vkGetBufferMemoryRequirements(device, buffer, &memRequirements);
            }
            void get(VkFormat& format, VkFormatProperties& props) {
                vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);
            }

            void get(VkPhysicalDeviceMemoryProperties& memProperties) {
                vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
            }

            void map(VkDeviceMemory& bufferMemory, VkDeviceSize size, void* &data) {
                vkMapMemory(device, bufferMemory, 0, size, 0, &data);
            }

            void unmap(VkDeviceMemory& bufferMemory) {
                vkUnmapMemory(device, bufferMemory);
            }

            void create(VkImageCreateInfo& imageInfo, VkImage& image) {
                if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
                    throw std::runtime_error("failed to create image!");
                }
            }

            void create(VkImageViewCreateInfo& viewInfo, VkImageView& imageView) {
                if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
                    throw std::runtime_error("failed to create texture image view!");
                }
            }

            void create(VkSamplerCreateInfo& samplerInfo, VkSampler& sampler) {
                if (vkCreateSampler(device, &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
                    throw std::runtime_error("failed to create texture sampler!");
                }
            }

            void create(VkSwapchainCreateInfoKHR& createInfo, VkSwapchainKHR& swapChain) {
                if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
                    throw std::runtime_error("failed to create swap chain!");
                }
            }

            void create(VkRenderPassCreateInfo& renderPassInfo, VkRenderPass& renderPass) {
                if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
                    throw std::runtime_error("failed to create render pass!");
                }
            }

            void create(VkDescriptorSetLayoutCreateInfo& layoutInfo, VkDescriptorSetLayout& descriptorSetLayout) {
                if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
                    throw std::runtime_error("failed to create descriptor set layout!");
                }
            }

            void create(VkPipelineLayoutCreateInfo& pipelineLayoutInfo, VkPipelineLayout& pipelineLayout) {
                if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
                    throw std::runtime_error("failed to create pipeline layout!");
                }
            }

            void create(VkGraphicsPipelineCreateInfo& pipelineInfo, VkPipeline& graphicsPipeline) {
                if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
                    throw std::runtime_error("failed to create graphics pipeline!");
                }
            }

            void create(VkShaderModuleCreateInfo& createInfo, VkShaderModule& shaderModule) {
                if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
                    throw std::runtime_error("failed to create shader module!");
                }
            }

            void create(VkBufferCreateInfo& bufferInfo, VkBuffer& buffer) {
                if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
                    throw std::runtime_error("failed to create buffer!");
                }
            }

            void create(VkFramebufferCreateInfo& framebufferInfo, VkFramebuffer& framebuffer) {
                if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &framebuffer) != VK_SUCCESS) {
                    throw std::runtime_error("failed to create framebuffer!");
                }
            }

            void create(VkCommandPoolCreateInfo& poolInfo, VkCommandPool& commandPool) {
                if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
                    throw std::runtime_error("failed to create graphics command pool!");
                }
            }

            void create(VkDescriptorPoolCreateInfo& poolInfo, VkDescriptorPool& descriptorPool) {
                if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
                    throw std::runtime_error("failed to create descriptor pool!");
                }
            }

            void create(VkSemaphoreCreateInfo& createInfo, VkSemaphore& semaphore) {
                if (vkCreateSemaphore(device, &createInfo, nullptr, &semaphore) != VK_SUCCESS) {
                    throw std::runtime_error("failed to create semaphore!");
                }
            }

            void create(VkFenceCreateInfo& createInfo, VkFence& fence) {
                if (vkCreateFence(device, &createInfo, nullptr, &fence) != VK_SUCCESS) {
                    throw std::runtime_error("failed to create fence!");
                }
            }

            void allocate(VkMemoryAllocateInfo& allocInfo, VkDeviceMemory& imageMemory) {
                if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
                    throw std::runtime_error("failed to allocate image memory!");
                }
            }

            void allocate(VkCommandBufferAllocateInfo& allocInfo, VkCommandBuffer* commandBuffers) {
                if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers) != VK_SUCCESS) {
                    throw std::runtime_error("failed to allocate command buffers!");
                }
            }

            void allocate(VkDescriptorSetAllocateInfo& allocInfo, VkDescriptorSet* descriptorSets) {
                if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSets) != VK_SUCCESS) {
                    throw std::runtime_error("failed to allocate descriptor sets!");
                }
            }

            void bind(VkImage& image, VkDeviceMemory& memory) {
                vkBindImageMemory(device, image, memory, 0);
            }

            void bind(VkBuffer& image, VkDeviceMemory& memory) {
                vkBindBufferMemory(device, image, memory, 0);
            }

            void submit(VkSubmitInfo& submitInfo, VkFence fence) {
                vkQueueSubmit(graphicsQueue, 1, &submitInfo, fence);
            }

            VkResult present(VkPresentInfoKHR& presentInfo) {
                return vkQueuePresentKHR(presentQueue, &presentInfo);
            }

            void waitQueue() {
                vkQueueWaitIdle(graphicsQueue);
            }

            void waitFence(VkFence& fence) {
                vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
            }

            void waitDevice() {
                vkDeviceWaitIdle(device);
            }

            VkResult nextImage(VkSwapchainKHR& swapchain, VkSemaphore& semaphore, uint32_t& imageIndex) {
                return vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, semaphore, VK_NULL_HANDLE, &imageIndex);
            }

            void reset(VkFence& fence) {
                vkResetFences(device, 1, &fence);
            }

            void reset(VkCommandBuffer& cmd) {
                vkResetCommandBuffer(cmd, 0);
            }

            void update(uint32_t size, VkWriteDescriptorSet* data) {
                vkUpdateDescriptorSets(device, size, data, 0, nullptr);
            }

            void free(VkDeviceMemory& imageMemory) {
                vkFreeMemory(device, imageMemory, nullptr);
            }

            void destroy(VkImage& image) {
                vkDestroyImage(device, image, nullptr);
            }

            void destroy(VkImageView& imageView) {
                vkDestroyImageView(device, imageView, nullptr);
            }

            void destroy(VkSampler& sampler) {
                vkDestroySampler(device, sampler, nullptr);
            }

            void destroy(VkBuffer& buffer) {
                vkDestroyBuffer(device, buffer, nullptr);
            }

            void destroy(VkFramebuffer& framebuffer) {
                vkDestroyFramebuffer(device, framebuffer, nullptr);
            }

            void destroy(VkSemaphore& semaphore) {
                vkDestroySemaphore(device, semaphore, nullptr);
            }

            void destroy(VkFence& fence) {
                vkDestroyFence(device, fence, nullptr);
            }

            void destroy(VkPipeline& pipeline) {
                vkDestroyPipeline(device, pipeline, nullptr);
            }

            void destroy(VkPipelineLayout& pipelineLayout) {
                vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
            }

            void destroy(VkRenderPass& renderPass) {
                vkDestroyRenderPass(device, renderPass, nullptr);
            }

            void free(VkCommandPool& commandPool, uint32_t size, VkCommandBuffer* data) {
                vkFreeCommandBuffers(device, commandPool, size, data);
            }

            void destroy(VkSwapchainKHR& swapchain) {
                vkDestroySwapchainKHR(device, swapchain, nullptr);
            }

            void destroy(VkDescriptorPool& descriptorPool) {
                vkDestroyDescriptorPool(device, descriptorPool, nullptr);
            }

            void destroy(VkDescriptorSetLayout& descriptorSetLayout) {
                vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
            }

            void destroy(VkCommandPool& commandPool) {
                vkDestroyCommandPool(device, commandPool, nullptr);
            }

            void destroy(VkShaderModule& shaderModule) {
                vkDestroyShaderModule(device, shaderModule, nullptr);
            }

            SwapChainSupportDetails querySwapChainSupport() {
                SwapChainSupportDetails details;

                vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, SurLoc::surface()->get(), &details.capabilities);

                uint32_t formatCount;
                vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, SurLoc::surface()->get(), &formatCount, nullptr);

                if (formatCount != 0) {
                    details.formats.resize(formatCount);
                    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, SurLoc::surface()->get(), &formatCount, details.formats.data());
                }

                uint32_t presentModeCount;
                vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, SurLoc::surface()->get(), &presentModeCount, nullptr);

                if (presentModeCount != 0) {
                    details.presentModes.resize(presentModeCount);
                    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, SurLoc::surface()->get(), &presentModeCount, details.presentModes.data());
                }

                return details;
            }

            QueueFamilyIndices findQueueFamilies() {
                QueueFamilyIndices indices;

                uint32_t queueFamilyCount = 0;
                vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

                std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
                vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

                int i = 0;
                for (const auto& queueFamily : queueFamilies) {
                    if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                        indices.graphicsFamily = i;
                    }

                    VkBool32 presentSupport = false;
                    vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, SurLoc::surface()->get(), &presentSupport);

                    if (presentSupport) {
                        indices.presentFamily = i;
                    }

                    if (indices.isComplete()) {
                        break;
                    }

                    i++;
                }

                return indices;
            }

            uint32_t find(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
                VkPhysicalDeviceMemoryProperties memProperties;
                get(memProperties);

                for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
                    if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                        return i;
                    }
                }

                throw std::runtime_error("failed to find suitable memory type!");
            }

            VkFormat find(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
                for (VkFormat format : candidates) {
                    VkFormatProperties props;
                    get(format, props);

                    if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
                        return format;
                    } else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
                        return format;
                    }
                }

                throw std::runtime_error("failed to find supported format!");
            }


        private:
            bool enableValidationLayers;

            VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
            VkDevice device;

            VkQueue graphicsQueue;
            VkQueue presentQueue;

            bool isDeviceSuitable(VkPhysicalDevice _physicalDevice) {
                QueueFamilyIndices indices = findQueueFamilies(_physicalDevice);

                bool extensionsSupported = checkDeviceExtensionSupport(_physicalDevice);

                bool swapChainAdequate = false;
                if (extensionsSupported) {
                    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(_physicalDevice);
                    swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
                }

                VkPhysicalDeviceFeatures supportedFeatures;
                vkGetPhysicalDeviceFeatures(_physicalDevice, &supportedFeatures);

                return indices.isComplete() && extensionsSupported && swapChainAdequate  && supportedFeatures.samplerAnisotropy;
            }

            bool checkDeviceExtensionSupport(VkPhysicalDevice _physicalDevice) {
                uint32_t extensionCount;
                vkEnumerateDeviceExtensionProperties(_physicalDevice, nullptr, &extensionCount, nullptr);

                std::vector<VkExtensionProperties> availableExtensions(extensionCount);
                vkEnumerateDeviceExtensionProperties(_physicalDevice, nullptr, &extensionCount, availableExtensions.data());

                std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

                for (const auto& extension : availableExtensions) {
                    requiredExtensions.erase(extension.extensionName);
                }

                return requiredExtensions.empty();
            }

            QueueFamilyIndices findQueueFamilies(VkPhysicalDevice _physicalDevice) {
                QueueFamilyIndices indices;

                uint32_t queueFamilyCount = 0;
                vkGetPhysicalDeviceQueueFamilyProperties(_physicalDevice, &queueFamilyCount, nullptr);

                std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
                vkGetPhysicalDeviceQueueFamilyProperties(_physicalDevice, &queueFamilyCount, queueFamilies.data());

                int i = 0;
                for (const auto& queueFamily : queueFamilies) {
                    if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                        indices.graphicsFamily = i;
                    }

                    VkBool32 presentSupport = false;
                    vkGetPhysicalDeviceSurfaceSupportKHR(_physicalDevice, i, SurLoc::surface()->get(), &presentSupport);

                    if (presentSupport) {
                        indices.presentFamily = i;
                    }

                    if (indices.isComplete()) {
                        break;
                    }

                    i++;
                }

                return indices;
            }

            SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice _physicalDevice) {
                SwapChainSupportDetails details;

                vkGetPhysicalDeviceSurfaceCapabilitiesKHR(_physicalDevice, SurLoc::surface()->get(), &details.capabilities);

                uint32_t formatCount;
                vkGetPhysicalDeviceSurfaceFormatsKHR(_physicalDevice, SurLoc::surface()->get(), &formatCount, nullptr);

                if (formatCount != 0) {
                    details.formats.resize(formatCount);
                    vkGetPhysicalDeviceSurfaceFormatsKHR(_physicalDevice, SurLoc::surface()->get(), &formatCount, details.formats.data());
                }

                uint32_t presentModeCount;
                vkGetPhysicalDeviceSurfacePresentModesKHR(_physicalDevice, SurLoc::surface()->get(), &presentModeCount, nullptr);

                if (presentModeCount != 0) {
                    details.presentModes.resize(presentModeCount);
                    vkGetPhysicalDeviceSurfacePresentModesKHR(_physicalDevice, SurLoc::surface()->get(), &presentModeCount, details.presentModes.data());
                }

                return details;
            }
    };
}
