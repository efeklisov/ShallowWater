#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <chrono>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <array>
#include <optional>
#include <set>

#include "instance.h"
#include "surface.h"
#include "vertex.h"
#include "device.h"
#include "read.h"
#include "create.h"
#include "command.h"
#include "texture.h"
#include "cubemap.h"
#include "shader.h"
#include "mesh.h"
#include "camera.h"
#include "imguiimpl.h"
#include "locator.h"
#include "swapchain.h"
#include "vertex.h"

const int WIDTH = 800;
const int HEIGHT = 600;

const int MAX_FRAMES_IN_FLIGHT = 2;

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

struct UniformBufferObject {
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};

class Application {
    public:
        void run() {
            initWindow();
            initVulkan();
            imgui = new ImGuiImpl(window);
            camera = new Camera(window, WIDTH, HEIGHT);
            mainLoop();
            cleanup();
        }

    private:
        GLFWwindow* window;
        ImGuiImpl* imgui;

        std::vector<VkFramebuffer> swapChainFramebuffers;

        VkRenderPass renderPass;
        VkDescriptorSetLayout descriptorSetLayout;

        float deltaTime = 0.0f;
        float lastFrame = 0.0f;

        Camera* camera;

        VkPipelineLayout pipelineLayout;

        VkPipeline graphicsPipeline;
        VkPipeline skyboxPipeline;

        std::vector<Mesh> meshes;

        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        VkBuffer vertexBuffer;
        VkDeviceMemory vertexBufferMemory;
        VkBuffer indexBuffer;
        VkDeviceMemory indexBufferMemory;

        VkDescriptorPool descriptorPool;

        std::vector<VkSemaphore> imageAvailableSemaphores;
        std::vector<VkSemaphore> renderFinishedSemaphores;
        std::vector<VkFence> inFlightFences;
        std::vector<VkFence> imagesInFlight;
        size_t currentFrame = 0;

        bool framebufferResized = false;

        void initWindow() {
            glfwInit();

            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

            window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
            glfwSetWindowUserPointer(window, this);
            glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);

            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            glfwSetCursorPosCallback(window, mouse_callback);
        }

        static void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
            auto app = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
            app->framebufferResized = true;
        }

        static void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
            auto app = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));

            app->camera->processMouse(xpos, ypos);
        }

        void initVulkan() {
            hw::loc::provide(new hw::Instance(enableValidationLayers));
            hw::loc::provide(new hw::Surface(window));
            hw::loc::provide(new hw::Device(enableValidationLayers));
            hw::loc::provide(new hw::Command());
            hw::loc::provide(new hw::SwapChain(window));

            hw::loc::provide(vertices);
            hw::loc::provide(indices);

            /**/meshes.push_back(Mesh("Skybox", "models/cube.obj", std::make_unique<CubeMap>("textures/storforsen")));
            /**/meshes.push_back(Mesh("Chalet", "models/chalet.obj", std::make_unique<Texture>("textures/chalet.jpg"),
                        glm::vec3(4.3177f, 1.8368f, 4.7955f), glm::vec3(-glm::pi<float>() / 2, 0.0f, 0.0f)));
            /**/meshes.push_back(Mesh("Lake", "models/lake.obj", std::make_unique<Texture>("textures/lake.png")));
            /**/createDescriptorSetLayout();

            createSwapChainRenderPass();
            createSwapChainFramebuffers();
            createSwapChainPipelines();

            /**/create::vertexBuffer(vertices, vertexBuffer, vertexBufferMemory);
            /**/create::indexBuffer(indices, indexBuffer, indexBufferMemory);

            createUniformBuffers();
            createDescriptorPool();
            allocateDescriptorSets();
            bindUnisToDescriptorSets();

            hw::loc::cmd()->createCommandBuffers(hw::loc::swapChain()->size());
            recordCommandBuffers();

            /**/createSyncObjects();
        }

        void mainLoop() {
            while (!glfwWindowShouldClose(window)) {
                glfwPollEvents();

                float currentFrame = glfwGetTime();
                deltaTime = currentFrame - lastFrame;
                lastFrame = currentFrame;

                camera->update(deltaTime);

                drawFrame();
            }

            hw::loc::device()->waitDevice();
        }

        void cleanupSwapChain() {
            for (auto framebuffer : swapChainFramebuffers) {
                hw::loc::device()->destroy(framebuffer);
            }

            hw::loc::cmd()->freeCommandBuffers();

            hw::loc::device()->destroy(graphicsPipeline);
            hw::loc::device()->destroy(skyboxPipeline);

            hw::loc::device()->destroy(pipelineLayout);
            hw::loc::device()->destroy(renderPass);

            imgui->cleanup();
            delete hw::loc::swapChain();

            for (auto& mesh: meshes)
                mesh.free();

            hw::loc::device()->destroy(descriptorPool);
        }

        void cleanup() {
            cleanupSwapChain();

            hw::loc::device()->destroy(descriptorSetLayout);

            hw::loc::device()->destroy(indexBuffer);
            hw::loc::device()->free(indexBufferMemory);

            hw::loc::device()->destroy(vertexBuffer);
            hw::loc::device()->free(vertexBufferMemory);

            for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                hw::loc::device()->destroy(renderFinishedSemaphores[i]);
                hw::loc::device()->destroy(imageAvailableSemaphores[i]);
                hw::loc::device()->destroy(inFlightFences[i]);
            }

            meshes.clear();
            delete imgui;
            delete hw::loc::cmd();
            delete hw::loc::device();
            delete hw::loc::surface();
            delete hw::loc::instance();
            delete camera;

            glfwDestroyWindow(window);

            glfwTerminate();
        }

        void recreateSwapChain() {
            int width = 0, height = 0;
            glfwGetFramebufferSize(window, &width, &height);
            while (width == 0 || height == 0) {
                glfwGetFramebufferSize(window, &width, &height);
                glfwWaitEvents();
            }

            hw::loc::device()->waitDevice();

            cleanupSwapChain();

            hw::loc::provide(new hw::SwapChain(window));
            createSwapChainRenderPass();
            createSwapChainFramebuffers();
            createSwapChainPipelines();

            createUniformBuffers();
            createDescriptorPool();
            allocateDescriptorSets();
            bindUnisToDescriptorSets();

            imgui->adjust();

            hw::loc::cmd()->createCommandBuffers(hw::loc::swapChain()->size());
            recordCommandBuffers();
        }

        void createSwapChainRenderPass() {
            VkAttachmentDescription colorAttachment = {};
            colorAttachment.format = hw::loc::swapChain()->format();
            colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
            colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            VkAttachmentDescription depthAttachment = {};
            depthAttachment.format = hw::loc::swapChain()->findDepthFormat();
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

            hw::loc::device()->create(renderPassInfo, renderPass);
        }

        void createDescriptorSetLayout() {
            VkDescriptorSetLayoutBinding uboLayoutBinding = {};
            uboLayoutBinding.binding = 0;
            uboLayoutBinding.descriptorCount = 1;
            uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            uboLayoutBinding.pImmutableSamplers = nullptr;
            uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

            VkDescriptorSetLayoutBinding samplerLayoutBinding = {};
            samplerLayoutBinding.binding = 1;
            samplerLayoutBinding.descriptorCount = 1;
            samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            samplerLayoutBinding.pImmutableSamplers = nullptr;
            samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

            std::array<VkDescriptorSetLayoutBinding, 2> bindings = {uboLayoutBinding, samplerLayoutBinding};
            VkDescriptorSetLayoutCreateInfo layoutInfo = {};
            layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
            layoutInfo.pBindings = bindings.data();

            hw::loc::device()->create(layoutInfo, descriptorSetLayout);

            for (auto& mesh: meshes)
                mesh.descriptor.layout = &descriptorSetLayout;
        }

        void createSwapChainPipelines() {
            Shader vertBase("shaders/base.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
            Shader fragBase("shaders/base.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
            Shader vertSkybox("shaders/skybox.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
            Shader fragSkybox("shaders/skybox.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

            VkPipelineShaderStageCreateInfo shaderStages[] = {vertSkybox.info(), fragSkybox.info()};

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
            viewport.width = (float) hw::loc::swapChain()->width();
            viewport.height = (float) hw::loc::swapChain()->height();
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;

            VkRect2D scissor = {};
            scissor.offset = {0, 0};
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
            colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
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

            std::vector<VkDescriptorSetLayout> layouts = {
                descriptorSetLayout
            };

            VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
            pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(layouts.size());
            pipelineLayoutInfo.pSetLayouts = layouts.data();

            hw::loc::device()->create(pipelineLayoutInfo, pipelineLayout);

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
            pipelineInfo.layout = pipelineLayout;
            pipelineInfo.renderPass = renderPass;
            pipelineInfo.subpass = 0;
            pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

            hw::loc::device()->create(pipelineInfo, skyboxPipeline);

            shaderStages[0] = vertBase.info();
            shaderStages[1] = fragBase.info();

            depthStencil.depthWriteEnable = VK_TRUE;
            depthStencil.depthTestEnable = VK_TRUE;

            hw::loc::device()->create(pipelineInfo, graphicsPipeline);

            for (auto& mesh: meshes)
                if (mesh.tag != "Skybox")
                    mesh.pipeline = &graphicsPipeline;
                else
                    meshes[0].pipeline = &skyboxPipeline;
        }

        void createSwapChainFramebuffers() {
            swapChainFramebuffers.resize(hw::loc::swapChain()->size());

            for (size_t i = 0; i < hw::loc::swapChain()->size(); i++) {
                std::array<VkImageView, 2> attachments = {
                    hw::loc::swapChain()->view(i),
                    hw::loc::swapChain()->depthView()
                };

                VkFramebufferCreateInfo framebufferInfo = {};
                framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
                framebufferInfo.renderPass = renderPass;
                framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
                framebufferInfo.pAttachments = attachments.data();
                framebufferInfo.width = hw::loc::swapChain()->width();
                framebufferInfo.height = hw::loc::swapChain()->height();
                framebufferInfo.layers = 1;

                hw::loc::device()->create(framebufferInfo, swapChainFramebuffers[i]);
            }
        }

        bool hasStencilComponent(VkFormat format) {
            return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
        }

        void createUniformBuffers() {
            VkDeviceSize bufferSize = sizeof(UniformBufferObject);

            for (auto& mesh: meshes) {
                mesh.uniform.buffers.resize(hw::loc::swapChain()->size());
                mesh.uniform.memory.resize(hw::loc::swapChain()->size());

                for (size_t i = 0; i < hw::loc::swapChain()->size(); i++) {
                    create::buffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                            | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, mesh.uniform.buffers[i], mesh.uniform.memory[i]);
                }
            }
        }

        void createDescriptorPool() {
            std::array<VkDescriptorPoolSize, 2> poolSizes = {};
            poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            poolSizes[0].descriptorCount = static_cast<uint32_t>(hw::loc::swapChain()->size() * meshes.size());
            poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            poolSizes[1].descriptorCount = static_cast<uint32_t>(hw::loc::swapChain()->size() * meshes.size());

            VkDescriptorPoolCreateInfo poolInfo = {};
            poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
            poolInfo.pPoolSizes = poolSizes.data();
            poolInfo.maxSets = static_cast<uint32_t>(hw::loc::swapChain()->size() * meshes.size());

            hw::loc::device()->create(poolInfo, descriptorPool);
        }

        void allocateDescriptorSets() {
            for (auto& mesh: meshes) {
                std::vector<VkDescriptorSetLayout> layouts(hw::loc::swapChain()->size(), *mesh.descriptor.layout);

                VkDescriptorSetAllocateInfo allocInfo = {};
                allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
                allocInfo.descriptorPool = descriptorPool;
                allocInfo.descriptorSetCount = static_cast<uint32_t>(hw::loc::swapChain()->size());
                allocInfo.pSetLayouts = layouts.data();

                mesh.descriptor.sets.resize(hw::loc::swapChain()->size());
                hw::loc::device()->allocate(allocInfo, mesh.descriptor.sets.data());
            }
        }

        void bindUnisToDescriptorSets() {
            for (size_t i = 0; i < hw::loc::swapChain()->size(); i++) {
                VkDescriptorBufferInfo bufferInfo = {};
                bufferInfo.offset = 0;
                bufferInfo.range = sizeof(UniformBufferObject);

                VkDescriptorImageInfo imageInfo = {};
                imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

                std::array<VkWriteDescriptorSet, 2> descriptorWrites = {};

                descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                descriptorWrites[0].dstBinding = 0;
                descriptorWrites[0].dstArrayElement = 0;
                descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                descriptorWrites[0].descriptorCount = 1;
                descriptorWrites[0].pBufferInfo = &bufferInfo;

                descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                descriptorWrites[1].dstBinding = 1;
                descriptorWrites[1].dstArrayElement = 0;
                descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                descriptorWrites[1].descriptorCount = 1;
                descriptorWrites[1].pImageInfo = &imageInfo;

                for (auto& mesh: meshes) {
                    descriptorWrites[0].dstSet = mesh.descriptor.sets[i];
                    descriptorWrites[1].dstSet = mesh.descriptor.sets[i];

                    bufferInfo.buffer = mesh.uniform.buffers[i];

                    imageInfo.imageView = mesh.texture->view();
                    imageInfo.sampler = mesh.texture->sampler();

                    hw::loc::device()->update(static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data());
                }
            }
        }

        void recordCommandBuffers() {
            for (size_t i = 0; i < hw::loc::swapChain()->size(); i++) {
                VkCommandBufferBeginInfo beginInfo = {};
                beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

                if (vkBeginCommandBuffer(hw::loc::cmd()->get(i), &beginInfo) != VK_SUCCESS) {
                    throw std::runtime_error("failed to begin recording command buffer!");
                }

                VkRenderPassBeginInfo renderPassInfo = {};
                renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
                renderPassInfo.renderPass = renderPass;
                renderPassInfo.framebuffer = swapChainFramebuffers[i];
                renderPassInfo.renderArea.offset = {0, 0};
                renderPassInfo.renderArea.extent = hw::loc::swapChain()->extent();

                std::array<VkClearValue, 2> clearValues = {};
                clearValues[0].color = {0.0f, 0.0f, 0.0f, 1.0f};
                clearValues[1].depthStencil = {1.0f, 0};

                renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
                renderPassInfo.pClearValues = clearValues.data();

                vkCmdBeginRenderPass(hw::loc::cmd()->get(i), &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

                VkDeviceSize offsets[] = {0};

                for (auto& mesh: meshes) {
                    vkCmdBindDescriptorSets(hw::loc::cmd()->get(i), VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipelineLayout, 0, 1, &mesh.descriptor.sets[i], 0, nullptr);
                    vkCmdBindVertexBuffers(hw::loc::cmd()->get(i), 0, 1, &vertexBuffer, offsets);
                    vkCmdBindIndexBuffer(hw::loc::cmd()->get(i), indexBuffer, 0, VK_INDEX_TYPE_UINT32);
                    vkCmdBindPipeline(hw::loc::cmd()->get(i), VK_PIPELINE_BIND_POINT_GRAPHICS, *mesh.pipeline);
                    vkCmdDrawIndexed(hw::loc::cmd()->get(i), mesh.vertex.size, 1, 0, mesh.vertex.start, 0);
                }

                vkCmdEndRenderPass(hw::loc::cmd()->get(i));

                if (vkEndCommandBuffer(hw::loc::cmd()->get(i)) != VK_SUCCESS) {
                    throw std::runtime_error("failed to record command buffer!");
                }
            }
        }

        void createSyncObjects() {
            imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
            renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
            inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
            imagesInFlight.resize(hw::loc::swapChain()->size(), VK_NULL_HANDLE);

            VkSemaphoreCreateInfo semaphoreInfo = {};
            semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

            VkFenceCreateInfo fenceInfo = {};
            fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

            for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                hw::loc::device()->create(semaphoreInfo, imageAvailableSemaphores[i]);
                hw::loc::device()->create(semaphoreInfo, renderFinishedSemaphores[i]);
                hw::loc::device()->create(fenceInfo, inFlightFences[i]);
            }
        }

        void updateUniformBuffer(uint32_t currentImage) {
            UniformBufferObject ubo = {};
            /* ubo.view = camera->view; */

            for (auto& mesh: meshes) {
                ubo.proj = camera->proj;
                glm::mat4 rotation = glm::mat4_cast(glm::normalize(glm::quat(mesh.rotation)));

                if (mesh.tag != "Skybox") {
                    ubo.view = glm::translate(camera->view, mesh.transform);
                } else ubo.view = glm::translate(camera->view, camera->cameraPos);
                ubo.model = rotation * glm::mat4(1.0f);

                if (mesh.tag != "Skybox") {
                    glm::vec3 scale = mesh.scale;
                    scale.x *= -1;
                    ubo.model = glm::scale(ubo.model, scale);
                } else ubo.model = glm::scale(ubo.model, mesh.scale);

                void* data;
                hw::loc::device()->map(mesh.uniform.memory[currentImage], sizeof(ubo), data);
                memcpy(data, &ubo, sizeof(ubo));
                hw::loc::device()->unmap(mesh.uniform.memory[currentImage]);
            }
        }

        void drawFrame() {
            // IMGUI
            ImGui_ImplVulkan_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            ImGui::Text("Skybox rotation");
            ImGui::Render();

            hw::loc::device()->waitFence(inFlightFences[currentFrame]);

            uint32_t imageIndex;
            VkResult result = hw::loc::device()->nextImage(hw::loc::swapChain()->get(), imageAvailableSemaphores[currentFrame], imageIndex);

            if (result == VK_ERROR_OUT_OF_DATE_KHR) {
                recreateSwapChain();
                ImGui_ImplVulkan_SetMinImageCount(hw::loc::swapChain()->size());
                return;
            } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
                throw std::runtime_error("failed to acquire swap chain image!");
            }

            updateUniformBuffer(imageIndex);
            camera->processInput();

            // Sync
            if (imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
                hw::loc::device()->waitFence(imagesInFlight[imageIndex]);
            }
            imagesInFlight[imageIndex] = inFlightFences[currentFrame];

            // Render IMGUI
            imgui->recordCommandBuffer(imageIndex);

            // Submit
            std::array<VkCommandBuffer, 2> submitCommandBuffers = {
                hw::loc::cmd()->get(imageIndex),
                imgui->getCommandBuffer(imageIndex)
            };

            VkSubmitInfo submitInfo = {};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

            VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
            VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
            submitInfo.waitSemaphoreCount = 1;
            submitInfo.pWaitSemaphores = waitSemaphores;
            submitInfo.pWaitDstStageMask = waitStages;

            submitInfo.commandBufferCount = static_cast<uint32_t>(submitCommandBuffers.size());
            submitInfo.pCommandBuffers = submitCommandBuffers.data();

            VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};
            submitInfo.signalSemaphoreCount = 1;
            submitInfo.pSignalSemaphores = signalSemaphores;

            hw::loc::device()->reset(inFlightFences[currentFrame]);

            hw::loc::device()->submit(submitInfo, inFlightFences[currentFrame]);

            // Present Frame
            VkPresentInfoKHR presentInfo = {};
            presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

            presentInfo.waitSemaphoreCount = 1;
            presentInfo.pWaitSemaphores = signalSemaphores;

            VkSwapchainKHR swapChains[] = {hw::loc::swapChain()->get()};
            presentInfo.swapchainCount = 1;
            presentInfo.pSwapchains = swapChains;

            presentInfo.pImageIndices = &imageIndex;

            result = hw::loc::device()->present(presentInfo);

            if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
                framebufferResized = false;
                recreateSwapChain();
                ImGui_ImplVulkan_SetMinImageCount(hw::loc::swapChain()->size());
            } else if (result != VK_SUCCESS) {
                throw std::runtime_error("failed to present swap chain image!");
            }

            currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
        }
};
