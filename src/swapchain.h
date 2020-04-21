#pragma once

#include <volk.h>

#include "create.h"
#include "device.h"
#include "locator.h"
#include "surface.h"

namespace hw {
class SwapChain {
public:
    SwapChain(GLFWwindow* _window)
        : window(_window)
    {
        createSwapChain();
        createSwapChainImageViews();
        createSwapChainDepthResources();
    }

    ~SwapChain()
    {
        for (uint32_t i = 0; i < depthImages.size(); i++) {
            hw::loc::device()->destroy(depthImageViews[i]);
            hw::loc::device()->destroy(depthImages[i]);
            hw::loc::device()->free(depthImageMemorys[i]);
        }

        for (auto framebuffer : swapChainFramebuffers) {
            hw::loc::device()->destroy(framebuffer);
        }

        for (auto imageView : swapChainImageViews) {
            hw::loc::device()->destroy(imageView);
        }

        hw::loc::device()->destroy(swapChain);
    }

    uint32_t size()
    {
        return swapChainImages.size();
    }

    uint32_t width()
    {
        return swapChainExtent.width;
    }

    uint32_t height()
    {
        return swapChainExtent.height;
    }

    VkSwapchainKHR& get()
    {
        return swapChain;
    }

    VkFormat& format()
    {
        return swapChainImageFormat;
    }

    VkImage& image(uint32_t index)
    {
        return swapChainImages[index];
    }

    VkImageView& view(uint32_t index)
    {
        return swapChainImageViews[index];
    }

    VkFramebuffer& frameBuffer(uint32_t index)
    {
        return swapChainFramebuffers[index];
    }


    VkImageView& depthView(uint32_t index)
    {
        return depthImageViews[index];
    }

    VkExtent2D& extent()
    {
        return swapChainExtent;
    }

    VkFormat findDepthFormat()
    {
        return hw::loc::device()->find(
            { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
    }

private:
    GLFWwindow* window;
    VkSwapchainKHR swapChain;
    VkExtent2D swapChainExtent;
    VkFormat swapChainImageFormat;
    std::vector<VkImage> swapChainImages;
    std::vector<VkImageView> swapChainImageViews;
    std::vector<VkFramebuffer> swapChainFramebuffers;

    std::vector<VkImage> depthImages;
    std::vector<VkImageView> depthImageViews;
    std::vector<VkDeviceMemory> depthImageMemorys;

    void createSwapChain()
    {
        auto swapChainSupport = hw::loc::device()->querySwapChainSupport();

        VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
        VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
        VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

        uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
        if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
            imageCount = swapChainSupport.capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = hw::loc::surface()->get();

        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        auto indices = hw::loc::device()->findQueueFamilies();
        uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

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

        hw::loc::device()->create(createInfo, swapChain);

        hw::loc::device()->get(swapChain, imageCount, nullptr);
        swapChainImages.resize(imageCount);
        hw::loc::device()->get(swapChain, imageCount, swapChainImages.data());

        swapChainImageFormat = surfaceFormat.format;
        swapChainExtent = extent;
        swapChainFramebuffers.resize(imageCount);
    }

    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
    {
        for (const auto& availableFormat : availableFormats) {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return availableFormat;
            }
        }

        return availableFormats[0];
    }

    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
    {
        for (const auto& availablePresentMode : availablePresentModes) {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                return availablePresentMode;
            }
            /* if (availablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR) { */
            /*     return availablePresentMode; */
            /* } */
        }

        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
    {
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

    void createSwapChainImageViews()
    {
        swapChainImageViews.resize(size());

        for (uint32_t i = 0; i < size(); i++) {
            swapChainImageViews[i] = create::imageView(image(i), format(), VK_IMAGE_ASPECT_COLOR_BIT);
        }
    }

    void createSwapChainDepthResources()
    {
        VkFormat depthFormat = findDepthFormat();

        depthImages.resize(swapChainImages.size());
        depthImageViews.resize(swapChainImages.size());
        depthImageMemorys.resize(swapChainImages.size());

        for (uint32_t i = 0; i < swapChainImages.size(); i++) {
            create::image(width(), height(), VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, depthImages[i], depthImageMemorys[i], depthFormat);
            depthImageViews[i] = create::imageView(depthImages[i], depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
        }
    }
};
}
