#pragma once
#include <volk.h>

#include <string_view>
#include <vector>

#include <create.h>
#include <locator.h>
#include <shader.h>
#include <swapchain.h>
#include <command.h>

class Compute {
    public:
        VkExtent2D& extent() {
            return cboExtent;
        }

        VkImage& color(u_int32_t frame, uint32_t index)
        {
            return colorImages[frame * hw::loc::swapChain()->size() + index];
        }

        VkImageView& colorView(u_int32_t frame, uint32_t index)
        {
            return colorImageViews[frame * hw::loc::swapChain()->size() + index];
        }

        VkSampler& colorSampler(u_int32_t frame, uint32_t index)
        {
            return colorSamplers[frame * hw::loc::swapChain()->size() + index];
        }

        VkPipeline& pipeline(uint32_t index)
        {
            return pipelines[index];
        }

        VkCommandBuffer& commandBuffer(uint32_t index)
        {
            return commandBuffers[index];
        }

        void addPipeline(VkPipelineLayout& layout, std::string_view compShader)
        {
            Shader comp(compShader.data(), VK_SHADER_STAGE_COMPUTE_BIT);

            initPipe(comp, layout);
        }

        Compute(std::string_view _tag, uint32_t imageCount=1, uint32_t width=300, uint32_t height=300)
            : tag(_tag) {

                initCBO(imageCount, width, height);
                hw::loc::comp()->createCommandBuffers(commandBuffers, hw::loc::swapChain()->size());
            }

        ~Compute() {
            hw::loc::comp()->freeCommandBuffers(commandBuffers);

            for (uint32_t i = 0; i < colorImages.size(); i++) {
                hw::loc::device()->destroy(colorImages[i]);
                hw::loc::device()->destroy(colorImageViews[i]);
                hw::loc::device()->destroy(colorSamplers[i]);
                hw::loc::device()->free(colorMemory[i]);
            }

            for (auto& pipe : pipelines) {
                hw::loc::device()->destroy(pipe);
            }
        }

        std::string tag;

    private:
        VkExtent2D cboExtent;

        std::vector<VkCommandBuffer> commandBuffers;
        std::vector<VkPipeline> pipelines;

        std::vector<VkImage> colorImages;
        std::vector<VkImageView> colorImageViews;
        std::vector<VkDeviceMemory> colorMemory;
        std::vector<VkSampler> colorSamplers;

        void initCBO(uint32_t imageCount, uint32_t width, uint32_t height) 
        {
            cboExtent = {width, height};

            colorImages.resize(imageCount * hw::loc::swapChain()->size());
            colorImageViews.resize(imageCount * hw::loc::swapChain()->size());
            colorMemory.resize(imageCount * hw::loc::swapChain()->size());
            colorSamplers.resize(imageCount * hw::loc::swapChain()->size());

            VkImageUsageFlags usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

            #pragma omp parallel for
            for (size_t i = 0; i < imageCount * hw::loc::swapChain()->size(); i++) {
                create::image(width, height, usage, colorImages[i], colorMemory[i], VK_FORMAT_R8G8B8A8_UNORM);
                colorImageViews[i] = create::imageView(colorImages[i], VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);
                create::sampler(colorSamplers[i]);
            }
        }

        void initPipe(Shader& shader, VkPipelineLayout& layout) {
            VkComputePipelineCreateInfo pipelineInfo = {};
            pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
            pipelineInfo.layout = layout;
            pipelineInfo.flags = 0;
            pipelineInfo.stage = shader.info();

            pipelines.resize(pipelines.size() + 1);
            hw::loc::device()->create(pipelineInfo, pipelines[pipelines.size() - 1]);
        }
};
