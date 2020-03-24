#pragma once

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#include <iostream>

#include "devloc.h"
#include "surface.h"
#include "create.h"

namespace hw {
    class SwapChain {
        public:
            SwapChain(GLFWwindow* _window, VkSurfaceKHR* _surface)
                : window(_window), surface(_surface) {
                    createSwapChain();
                    createImageViews();
                    createDepthResources();
                    createRenderPass();
                    createFramebuffers();
                }

            ~SwapChain() {
                DevLoc::device()->destroy(depthImageView);
                DevLoc::device()->destroy(depthImage);
                DevLoc::device()->free(depthImageMemory);

                for (auto framebuffer : swapChainFramebuffers) {
                    DevLoc::device()->destroy(framebuffer);
                }

                for (auto imageView : swapChainImageViews) {
                    DevLoc::device()->destroy(imageView);
                }

                DevLoc::device()->destroy(renderPass);
                DevLoc::device()->destroy(swapChain);

            }

            VkSwapchainKHR& get() {
                return swapChain;
            }

            VkImageView& getImageView(uint32_t index) {
                return swapChainImageViews[index];
            }

            VkFormat& getImageFormat() {
                return swapChainImageFormat;
            }

            VkFramebuffer& getFrameBuffer(uint32_t index) {
                return swapChainFramebuffers[index];
            }

            VkExtent2D& getExtent() {
                return swapChainExtent;
            }

            VkRenderPass& getRenderPass() {
                return renderPass;
            }

            uint32_t width() {
                return swapChainExtent.width;
            }

            uint32_t height() {
                return swapChainExtent.height;
            }

            uint32_t size() {
                return swapChainImages.size();
            }

            VkFormat findDepthFormat() {
                return DevLoc::device()->find(
                        {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
                        VK_IMAGE_TILING_OPTIMAL,
                        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
                        );
            }

        private:
            GLFWwindow* window;
            VkSurfaceKHR* surface;

            VkImage depthImage;
            VkDeviceMemory depthImageMemory;
            VkImageView depthImageView;

            VkRenderPass renderPass;

            VkSwapchainKHR swapChain;
            std::vector<VkImage> swapChainImages;
            VkFormat swapChainImageFormat;
            VkExtent2D swapChainExtent;
            std::vector<VkImageView> swapChainImageViews;
            std::vector<VkFramebuffer> swapChainFramebuffers;

            void createSwapChain() {
                auto swapChainSupport = DevLoc::device()->querySwapChainSupport();

                VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
                VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
                VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

                uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
                if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
                    imageCount = swapChainSupport.capabilities.maxImageCount;
                }

                VkSwapchainCreateInfoKHR createInfo = {};
                createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
                createInfo.surface = *surface;

                createInfo.minImageCount = imageCount;
                createInfo.imageFormat = surfaceFormat.format;
                createInfo.imageColorSpace = surfaceFormat.colorSpace;
                createInfo.imageExtent = extent;
                createInfo.imageArrayLayers = 1;
                createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

                auto indices = DevLoc::device()->findQueueFamilies();
                uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

                if (indices.graphicsFamily != indices.presentFamily) {
                    createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
                    createInfo.queueFamilyIndexCount = 2;
                    createInfo.pQueueFamilyIndices = queueFamilyIndices;
                } else {
                    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
                }

                createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
                createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
                createInfo.presentMode = presentMode;
                createInfo.clipped = VK_TRUE;

                DevLoc::device()->create(createInfo, swapChain);

                DevLoc::device()->get(swapChain, imageCount, nullptr);
                swapChainImages.resize(imageCount);
                DevLoc::device()->get(swapChain, imageCount, swapChainImages.data());

                swapChainImageFormat = surfaceFormat.format;
                swapChainExtent = extent;
            }

            void createImageViews() {
                swapChainImageViews.resize(swapChainImages.size());

                for (uint32_t i = 0; i < swapChainImages.size(); i++) {
                    swapChainImageViews[i] = create::imageView(swapChainImages[i], swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
                }
            }

            void createDepthResources() {
                VkFormat depthFormat = findDepthFormat();

                create::image(swapChainExtent.width, swapChainExtent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL,
                        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depthImage, depthImageMemory);
                depthImageView = create::imageView(depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
            }

            void createFramebuffers() {
                swapChainFramebuffers.resize(swapChainImageViews.size());

                for (size_t i = 0; i < swapChainImageViews.size(); i++) {
                    std::array<VkImageView, 2> attachments = {
                        swapChainImageViews[i],
                        depthImageView
                    };

                    VkFramebufferCreateInfo framebufferInfo = {};
                    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
                    framebufferInfo.renderPass = renderPass;
                    framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
                    framebufferInfo.pAttachments = attachments.data();
                    framebufferInfo.width = swapChainExtent.width;
                    framebufferInfo.height = swapChainExtent.height;
                    framebufferInfo.layers = 1;

                    DevLoc::device()->create(framebufferInfo, swapChainFramebuffers[i]);
                }
            }

            void createRenderPass() {
                VkAttachmentDescription colorAttachment = {};
                colorAttachment.format = getImageFormat();
                colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
                colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
                colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
                colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
                colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

                VkAttachmentDescription depthAttachment = {};
                depthAttachment.format = findDepthFormat();
                depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
                depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
                depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
                depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
                depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

                VkAttachmentReference colorAttachmentRef = {};
                colorAttachmentRef.attachment = 0;
                colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

                VkAttachmentReference depthAttachmentRef = {};
                depthAttachmentRef.attachment = 1;
                depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

                VkSubpassDescription subpass = {};
                subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
                subpass.colorAttachmentCount = 1;
                subpass.pColorAttachments = &colorAttachmentRef;
                subpass.pDepthStencilAttachment = &depthAttachmentRef;

                VkSubpassDependency dependency = {};
                dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
                dependency.dstSubpass = 0;
                dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                dependency.srcAccessMask = 0;
                dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

                std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};
                VkRenderPassCreateInfo renderPassInfo = {};
                renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
                renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
                renderPassInfo.pAttachments = attachments.data();
                renderPassInfo.subpassCount = 1;
                renderPassInfo.pSubpasses = &subpass;
                renderPassInfo.dependencyCount = 1;
                renderPassInfo.pDependencies = &dependency;

                DevLoc::device()->create(renderPassInfo, renderPass);
            }

            VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
                for (const auto& availableFormat : availableFormats) {
                    if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
                            availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                        return availableFormat;
                    }
                }

                return availableFormats[0];
            }

            VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
                for (const auto& availablePresentMode : availablePresentModes) {
                    if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                        return availablePresentMode;
                    }
                }

                return VK_PRESENT_MODE_FIFO_KHR;
            }

            VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
                if (capabilities.currentExtent.width != UINT32_MAX) {
                    return capabilities.currentExtent;
                } else {
                    int width, height;
                    glfwGetFramebufferSize(window, &width, &height);

                    VkExtent2D actualExtent = {
                        static_cast<uint32_t>(width),
                        static_cast<uint32_t>(height)
                    };

                    actualExtent.width = std::max(capabilities.minImageExtent.width,
                            std::min(capabilities.maxImageExtent.width, actualExtent.width));
                    actualExtent.height = std::max(capabilities.minImageExtent.height,
                            std::min(capabilities.maxImageExtent.height, actualExtent.height));

                    return actualExtent;
                }
            }
    };
}
