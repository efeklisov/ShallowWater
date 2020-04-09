#pragma once
#include <volk.h>

#include <string_view>
#include <vector>

#include <create.h>
#include <locator.h>
#include <shader.h>
#include <swapchain.h>
#include <command.h>

class Render {
    public:
        VkImageView& colorView(uint32_t index)
        {
            if (boolmap.haveFBO)
                return colorImageViews[index];
            else if (boolmap.havedefaultFBO)
                return hw::loc::swapChain()->view(index);
            throw std::runtime_error("No FBO assigned");
        }

        VkSampler& colorSampler(uint32_t index)
        {
            if (boolmap.haveFBO)
                return colorSamplers[index];
            throw std::runtime_error("Not available with defaultFBO");
        }

        VkImageView& depthView(uint32_t index)
        {
            if (boolmap.haveFBO)
                return depthImageViews[index];
            else if (boolmap.havedefaultFBO)
                return hw::loc::swapChain()->depthView(index);
            throw std::runtime_error("No FBO assigned");
        }

        VkSampler& depthSampler(uint32_t index)
        {
            if (boolmap.haveFBO)
                return depthSamplers[index];
            throw std::runtime_error("Not available with defaultFBO");
        }

        VkFramebuffer& frameBuffer(uint32_t& index)
        {
            if (boolmap.haveFBO)
                return frameBuffers[index];
            else if (boolmap.havedefaultFBO)
                return hw::loc::swapChain()->frameBuffer(index);
            throw std::runtime_error("No FBO assigned");
        }

        VkCommandBuffer& commandBuffer(uint32_t index)
        {
            return commandBuffers[index];
        }

        VkPipeline& pipeline(uint32_t index)
        {
            return pipelines[index];
        }

        VkRenderPass& renderPass()
        {
            return pass;
        }

        void addPipeline(VkPipelineLayout& layout, std::string_view vertShader, std::string_view fragShader, bool checkDepth = true)
        {
            Shader vert(vertShader.data(), VK_SHADER_STAGE_VERTEX_BIT);
            Shader frag(fragShader.data(), VK_SHADER_STAGE_FRAGMENT_BIT);

            VkPipelineShaderStageCreateInfo shaderStages[] = { vert.info(), frag.info() };

            VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
            vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

            auto bindingDescription = Vertex::getBindingDescription();
            auto attributeDescriptions = Vertex::getAttributeDescriptions();

            vertexInputInfo.vertexBindingDescriptionCount = 1;
            vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
            vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
            vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

            VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
            inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
            inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            inputAssembly.primitiveRestartEnable = VK_FALSE;

            VkViewport viewport = {};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = (float)hw::loc::swapChain()->width();
            viewport.height = (float)hw::loc::swapChain()->height();
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;

            VkRect2D scissor = {};
            scissor.offset = { 0, 0 };
            scissor.extent = hw::loc::swapChain()->extent();

            VkPipelineViewportStateCreateInfo viewportState = {};
            viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
            viewportState.viewportCount = 1;
            viewportState.pViewports = &viewport;
            viewportState.scissorCount = 1;
            viewportState.pScissors = &scissor;

            VkPipelineRasterizationStateCreateInfo rasterizer = {};
            rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
            rasterizer.depthClampEnable = VK_FALSE;
            rasterizer.rasterizerDiscardEnable = VK_FALSE;
            rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
            rasterizer.lineWidth = 1.0f;
            rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
            rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
            rasterizer.depthBiasEnable = VK_FALSE;

            VkPipelineMultisampleStateCreateInfo multisampling = {};
            multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
            multisampling.sampleShadingEnable = VK_FALSE;
            multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

            VkPipelineDepthStencilStateCreateInfo depthStencil = {};
            depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
            depthStencil.depthTestEnable = VK_FALSE;
            depthStencil.depthWriteEnable = VK_FALSE;
            depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
            depthStencil.depthBoundsTestEnable = VK_FALSE;
            depthStencil.stencilTestEnable = VK_FALSE;

            VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
            colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            colorBlendAttachment.blendEnable = VK_FALSE;

            VkPipelineColorBlendStateCreateInfo colorBlending = {};
            colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            colorBlending.logicOpEnable = VK_FALSE;
            colorBlending.logicOp = VK_LOGIC_OP_COPY;
            colorBlending.attachmentCount = 1;
            colorBlending.pAttachments = &colorBlendAttachment;
            colorBlending.blendConstants[0] = 0.0f;
            colorBlending.blendConstants[1] = 0.0f;
            colorBlending.blendConstants[2] = 0.0f;
            colorBlending.blendConstants[3] = 0.0f;

            VkGraphicsPipelineCreateInfo pipelineInfo = {};
            pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
            pipelineInfo.stageCount = 2;
            pipelineInfo.pStages = shaderStages;
            pipelineInfo.pVertexInputState = &vertexInputInfo;
            pipelineInfo.pInputAssemblyState = &inputAssembly;
            pipelineInfo.pViewportState = &viewportState;
            pipelineInfo.pRasterizationState = &rasterizer;
            pipelineInfo.pMultisampleState = &multisampling;
            pipelineInfo.pDepthStencilState = &depthStencil;
            pipelineInfo.pColorBlendState = &colorBlending;
            pipelineInfo.layout = layout;
            pipelineInfo.renderPass = pass;
            pipelineInfo.subpass = 0;
            pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

            if (checkDepth) {
                depthStencil.depthWriteEnable = VK_TRUE;
                depthStencil.depthTestEnable = VK_TRUE;
            }

            pipelines.resize(pipelines.size() + 1);
            hw::loc::device()->create(pipelineInfo, pipelines[pipelines.size() - 1]);
        }

        void setToDefaultFBO()
        {
            assert(!boolmap.haveFBO);

            for (size_t i = 0; i < hw::loc::swapChain()->size(); i++) {
                std::array<VkImageView, 2> attachments = {
                    hw::loc::swapChain()->view(i),
                    hw::loc::swapChain()->depthView(i)
                };

                VkFramebufferCreateInfo framebufferInfo = {};
                framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
                framebufferInfo.renderPass = pass;
                framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
                framebufferInfo.pAttachments = attachments.data();
                framebufferInfo.width = hw::loc::swapChain()->width();
                framebufferInfo.height = hw::loc::swapChain()->height();
                framebufferInfo.layers = 1;

                hw::loc::device()->create(framebufferInfo, hw::loc::swapChain()->frameBuffer(i));
            }

            boolmap.havedefaultFBO = true;
        }

        void initFBO(uint32_t width = hw::loc::swapChain()->width(), uint32_t height = hw::loc::swapChain()->height()) 
        {
            assert(!boolmap.havedefaultFBO);

            frameBuffers.resize(hw::loc::swapChain()->size());

            colorImages.resize(frameBuffers.size());
            colorImageViews.resize(frameBuffers.size());
            colorMemory.resize(frameBuffers.size());
            colorSamplers.resize(frameBuffers.size());

            depthImages.resize(frameBuffers.size());
            depthImageViews.resize(frameBuffers.size());
            depthMemory.resize(frameBuffers.size());
            depthSamplers.resize(frameBuffers.size());

            auto depthFormat = hw::loc::swapChain()->findDepthFormat();

            for (size_t i = 0; i < frameBuffers.size(); i++) {
                create::image(width, height, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, colorImages[i], colorMemory[i], hw::loc::swapChain()->format());
                colorImageViews[i] = create::imageView(colorImages[i], hw::loc::swapChain()->format(), VK_IMAGE_ASPECT_COLOR_BIT);
                create::sampler(colorSamplers[i]);

                create::image(width, height, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, depthImages[i], depthMemory[i], depthFormat);
                depthImageViews[i] = create::imageView(depthImages[i], depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
                create::sampler(depthSamplers[i]);

                std::array<VkImageView, 2> attachments = {
                    colorImageViews[i],
                    depthImageViews[i]
                };

                VkFramebufferCreateInfo framebufferInfo = {};
                framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
                framebufferInfo.renderPass = pass;
                framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
                framebufferInfo.pAttachments = attachments.data();
                framebufferInfo.width = width;
                framebufferInfo.height = height;
                framebufferInfo.layers = 1;

                hw::loc::device()->create(framebufferInfo, frameBuffers[i]);
            }

            boolmap.haveFBO = true;
        }

        Render(std::string_view _tag, VkImageLayout colorFinal = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VkImageLayout depthFinal = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
            : tag(_tag) {
                initPass(colorFinal, depthFinal);
                hw::loc::cmd()->createCommandBuffers(commandBuffers, hw::loc::swapChain()->size());
            }

        ~Render() {
            hw::loc::cmd()->freeCommandBuffers(commandBuffers);

            if (boolmap.haveFBO) {
                for (uint32_t i = 0; i < frameBuffers.size(); i++) {
                    hw::loc::device()->destroy(colorImages[i]);
                    hw::loc::device()->destroy(colorImageViews[i]);
                    hw::loc::device()->destroy(colorSamplers[i]);
                    hw::loc::device()->free(colorMemory[i]);

                    hw::loc::device()->destroy(depthImages[i]);
                    hw::loc::device()->destroy(depthImageViews[i]);
                    hw::loc::device()->destroy(depthSamplers[i]);
                    hw::loc::device()->free(depthMemory[i]);

                    hw::loc::device()->destroy(frameBuffers[i]);
                }
            }

            for (auto& pipe : pipelines) {
                hw::loc::device()->destroy(pipe);
            }

            hw::loc::device()->destroy(pass);
        }

        std::string tag;

    private:
        struct StructAvailableResourses {
            bool havedefaultFBO = false;
            bool haveFBO = false;
        } boolmap;

        VkRenderPass pass;
        std::vector<VkFramebuffer> frameBuffers;
        std::vector<VkCommandBuffer> commandBuffers;
        std::vector<VkPipeline> pipelines;

        std::vector<VkImage> colorImages;
        std::vector<VkImageView> colorImageViews;
        std::vector<VkDeviceMemory> colorMemory;
        std::vector<VkSampler> colorSamplers;

        std::vector<VkImage> depthImages;
        std::vector<VkImageView> depthImageViews;
        std::vector<VkDeviceMemory> depthMemory;
        std::vector<VkSampler> depthSamplers;

        void initPass(VkImageLayout colorFinal, VkImageLayout depthFinal)
        {
            VkAttachmentDescription colorAttachment = {};
            colorAttachment.format = hw::loc::swapChain()->format();
            colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
            colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            colorAttachment.finalLayout = colorFinal;

            VkAttachmentDescription depthAttachment = {};
            depthAttachment.format = hw::loc::swapChain()->findDepthFormat();
            depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
            depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            depthAttachment.finalLayout = depthFinal;

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

            std::vector<VkAttachmentDescription> attachments = { colorAttachment, depthAttachment };
            VkRenderPassCreateInfo renderPassInfo = {};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
            renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
            renderPassInfo.pAttachments = attachments.data();
            renderPassInfo.subpassCount = 1;
            renderPassInfo.pSubpasses = &subpass;
            renderPassInfo.dependencyCount = 1;
            renderPassInfo.pDependencies = &dependency;

            hw::loc::device()->create(renderPassInfo, pass);
        }

};
