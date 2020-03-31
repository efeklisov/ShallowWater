#pragma once

#include <volk.h>
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include <stdexcept>

#include "locator.h"
#include "device.h"
#include "swapchain.h"

class ImGuiImpl {
    public:
        ImGuiImpl(GLFWwindow* _window) : window(_window) {
            initImgui();
        }

        ~ImGuiImpl() {
            ImGui_ImplVulkan_Shutdown();
            ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext();

            hw::loc::device()->destroy(imguiDescriptorPool);
            delete imguicmd;
        }

        void cleanup() {
            for (auto framebuffer : imguiFramebuffers) {
                hw::loc::device()->destroy(framebuffer);
            }

            imguicmd->freeCommandBuffers();
            hw::loc::device()->destroy(imguiRenderPass);
        }

        void adjust() {
            createImguiRenderPass();
            createImguiFramebuffers();

            imguicmd->createCommandBuffers(hw::loc::swapChain()->size());
        }

        VkCommandBuffer& getCommandBuffer(uint32_t index) {
            return imguicmd->get(index);
        }

        void recordCommandBuffer(uint32_t imageIndex) {
            VkCommandBufferBeginInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            if (vkBeginCommandBuffer(imguicmd->get(imageIndex), &info) != VK_SUCCESS) {
                throw std::runtime_error("failed to begin imgui command buffer");
            }

            VkRenderPassBeginInfo beginInfo = {};
            beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            beginInfo.renderPass = imguiRenderPass;
            beginInfo.framebuffer = imguiFramebuffers[imageIndex];
            beginInfo.renderArea.extent.width = hw::loc::swapChain()->width();
            beginInfo.renderArea.extent.height = hw::loc::swapChain()->height();

            VkClearValue clearValue;
            clearValue.color = {1.0f, 1.0f, 1.0f, 1.0f};

            beginInfo.clearValueCount = 1;
            beginInfo.pClearValues = &clearValue;
            vkCmdBeginRenderPass(imguicmd->get(imageIndex), &beginInfo, VK_SUBPASS_CONTENTS_INLINE);

            ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), imguicmd->get(imageIndex));

            vkCmdEndRenderPass(imguicmd->get(imageIndex));
            if (vkEndCommandBuffer(imguicmd->get(imageIndex)) != VK_SUCCESS) {
                throw std::runtime_error("failed to end imgui command buffer");
            }
        }

    private:
        GLFWwindow* window;
        std::vector<VkFramebuffer> imguiFramebuffers;
        VkRenderPass imguiRenderPass;
        hw::Command* imguicmd;
        VkDescriptorPool imguiDescriptorPool;

        void initImgui() {
            IMGUI_CHECKVERSION();
            ImGui::CreateContext();
            ImGuiIO& io = ImGui::GetIO(); (void)io;
            ImGui::StyleColorsDark();

            createImguiDescriptorPool();
            createImguiRenderPass();
            createImguiFramebuffers();

            hw::QueueFamilyIndices queueFamilyIndices = hw::loc::device()->findQueueFamilies();

            ImGui_ImplGlfw_InitForVulkan(window, true);
            ImGui_ImplVulkan_InitInfo info = {};
            info.Instance = hw::loc::instance()->get();
            info.PhysicalDevice = hw::loc::device()->getPhysical();
            info.Device = hw::loc::device()->getLogical();
            info.QueueFamily = queueFamilyIndices.graphicsFamily.value();
            info.Queue = hw::loc::device()->getGraphicsQueue();
            info.PipelineCache = VK_NULL_HANDLE;
            info.DescriptorPool = imguiDescriptorPool;
            info.Allocator = VK_NULL_HANDLE;
            info.MinImageCount = hw::loc::swapChain()->size();
            info.ImageCount = hw::loc::swapChain()->size();
            info.CheckVkResultFn = checkVKresult;
            ImGui_ImplVulkan_Init(&info, imguiRenderPass);

            hw::loc::cmd()->customSingleCommand(ImGui_ImplVulkan_CreateFontsTexture);
            ImGui_ImplVulkan_DestroyFontUploadObjects();

            imguicmd = new hw::Command(VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
            imguicmd->createCommandBuffers(hw::loc::swapChain()->size());
        }

        void createImguiFramebuffers() {
            VkImageView attachment[1];

            VkFramebufferCreateInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            info.renderPass = imguiRenderPass;
            info.attachmentCount = 1;
            info.pAttachments = attachment;
            info.width = hw::loc::swapChain()->width();
            info.height = hw::loc::swapChain()->height();
            info.layers = 1;

            imguiFramebuffers.resize(hw::loc::swapChain()->size());
            for (uint32_t i = 0; i < hw::loc::swapChain()->size(); i++)
            {
                attachment[0] = hw::loc::swapChain()->view(i);
                hw::loc::device()->create(info, imguiFramebuffers[i]);
            }
        }

        void createImguiRenderPass() {
            VkAttachmentDescription attachment = {};
            attachment.format = hw::loc::swapChain()->format();
            attachment.samples = VK_SAMPLE_COUNT_1_BIT;
            attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
            attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

            VkAttachmentReference color_attachment = {};
            color_attachment.attachment = 0;
            color_attachment.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            VkSubpassDescription subpass = {};
            subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpass.colorAttachmentCount = 1;
            subpass.pColorAttachments = &color_attachment;

            VkSubpassDependency dependency = {};
            dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
            dependency.dstSubpass = 0;
            dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependency.srcAccessMask = 0;
            dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

            VkRenderPassCreateInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
            info.attachmentCount = 1;
            info.pAttachments = &attachment;
            info.subpassCount = 1;
            info.pSubpasses = &subpass;
            info.dependencyCount = 1;
            info.pDependencies = &dependency;
            hw::loc::device()->create(info, imguiRenderPass);
        }

        void createImguiDescriptorPool() {
            VkDescriptorPoolSize size[] =
            {
                { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
                { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
                { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
                { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
                { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
                { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
                { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
                { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
                { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
                { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
                { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
            };

            VkDescriptorPoolCreateInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
            info.maxSets = 1000 * IM_ARRAYSIZE(size);
            info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(size);
            info.pPoolSizes = size;
            hw::loc::device()->create(info, imguiDescriptorPool);
        }

        static void checkVKresult(VkResult err)
        {
            if (err == 0) return;
            printf("VkResult %d\n", err);
        }
};
