#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

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
#include "insloc.h"
#include "surloc.h"
#include "devloc.h"
#include "cmdloc.h"
#include "swploc.h"
#include "create.h"
#include "command.h"
#include "texture.h"
#include "cubemap.h"
#include "shader.h"
#include "mesh.h"
#include "camera.h"
#include "vertexloc.h"

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
            initImgui();
            camera = new Camera(window, WIDTH, HEIGHT);
            mainLoop();
            cleanup();
        }

    private:
        GLFWwindow* window;

        std::vector<VkFramebuffer> swapChainFramebuffers;

        std::vector<VkFramebuffer> imguiFramebuffers;

        VkRenderPass renderPass;
        VkRenderPass imguiRenderPass;

        VkDescriptorSetLayout descriptorSetLayout;

        float deltaTime = 0.0f;
        float lastFrame = 0.0f;

        Camera* camera;

        VkPipelineLayout pipelineLayout;

        VkPipeline graphicsPipeline;
        VkPipeline skyboxPipeline;

        hw::Command* imguicmd;

        std::vector<Mesh> meshes;

        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        VkBuffer vertexBuffer;
        VkDeviceMemory vertexBufferMemory;
        VkBuffer indexBuffer;
        VkDeviceMemory indexBufferMemory;

        VkDescriptorPool descriptorPool;
        VkDescriptorPool imguiDescriptorPool;

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
            InsLoc::provide(new hw::Instance(enableValidationLayers));
            SurLoc::provide(new hw::Surface(window));
            DevLoc::provide(new hw::Device(enableValidationLayers));
            CmdLoc::provide(new hw::Command());
            SwpLoc::provide(new hw::SwapChain(window));

            VertexLoc::provide(vertices);
            VertexLoc::provide(indices);

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

            CmdLoc::cmd()->createCommandBuffers(SwpLoc::swapChain()->size());
            recordCommandBuffers();

            /**/createSyncObjects();
        }

        void initImgui() {
            IMGUI_CHECKVERSION();
            ImGui::CreateContext();
            ImGuiIO& io = ImGui::GetIO(); (void)io;
            ImGui::StyleColorsDark();

            createImguiDescriptorPool();
            createImguiRenderPass();
            createImguiFramebuffers();

            hw::QueueFamilyIndices queueFamilyIndices = DevLoc::device()->findQueueFamilies();

            ImGui_ImplGlfw_InitForVulkan(window, true);
            ImGui_ImplVulkan_InitInfo info = {};
            info.Instance = InsLoc::instance()->get();
            info.PhysicalDevice = DevLoc::device()->getPhysical();
            info.Device = DevLoc::device()->getLogical();
            info.QueueFamily = queueFamilyIndices.graphicsFamily.value();
            info.Queue = DevLoc::device()->getGraphicsQueue();
            info.PipelineCache = VK_NULL_HANDLE;
            info.DescriptorPool = imguiDescriptorPool;
            info.Allocator = VK_NULL_HANDLE;
            info.MinImageCount = SwpLoc::swapChain()->size();
            info.ImageCount = SwpLoc::swapChain()->size();
            info.CheckVkResultFn = checkVKresult;
            ImGui_ImplVulkan_Init(&info, imguiRenderPass);

            CmdLoc::cmd()->customSingleCommand(ImGui_ImplVulkan_CreateFontsTexture);
            ImGui_ImplVulkan_DestroyFontUploadObjects();

            imguicmd = new hw::Command(VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
            imguicmd->createCommandBuffers(SwpLoc::swapChain()->size());
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

            DevLoc::device()->waitDevice();
        }

        void cleanupSwapChain() {
            for (auto framebuffer : swapChainFramebuffers) {
                DevLoc::device()->destroy(framebuffer);
            }

            for (auto framebuffer : imguiFramebuffers) {
                DevLoc::device()->destroy(framebuffer);
            }

            CmdLoc::cmd()->freeCommandBuffers();
            imguicmd->freeCommandBuffers();

            DevLoc::device()->destroy(graphicsPipeline);
            DevLoc::device()->destroy(skyboxPipeline);

            DevLoc::device()->destroy(pipelineLayout);
            DevLoc::device()->destroy(renderPass);
            DevLoc::device()->destroy(imguiRenderPass);

            delete SwpLoc::swapChain();

            for (auto& mesh: meshes)
                mesh.free();

            DevLoc::device()->destroy(descriptorPool);
        }

        void cleanup() {
            cleanupSwapChain();

            DevLoc::device()->destroy(descriptorSetLayout);

            DevLoc::device()->destroy(indexBuffer);
            DevLoc::device()->free(indexBufferMemory);

            DevLoc::device()->destroy(vertexBuffer);
            DevLoc::device()->free(vertexBufferMemory);

            for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                DevLoc::device()->destroy(renderFinishedSemaphores[i]);
                DevLoc::device()->destroy(imageAvailableSemaphores[i]);
                DevLoc::device()->destroy(inFlightFences[i]);
            }

            ImGui_ImplVulkan_Shutdown();
            ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext();

            DevLoc::device()->destroy(imguiDescriptorPool);
            delete imguicmd;

            meshes.clear();
            delete CmdLoc::cmd();
            delete DevLoc::device();
            delete SurLoc::surface();
            delete InsLoc::instance();
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

            DevLoc::device()->waitDevice();

            cleanupSwapChain();

            SwpLoc::provide(new hw::SwapChain(window));
            createSwapChainRenderPass();
            createSwapChainFramebuffers();
            createSwapChainPipelines();

            createUniformBuffers();
            createDescriptorPool();
            allocateDescriptorSets();
            bindUnisToDescriptorSets();

            CmdLoc::cmd()->createCommandBuffers(SwpLoc::swapChain()->size());
            recordCommandBuffers();

            createImguiRenderPass();
            createImguiFramebuffers();

            imguicmd->createCommandBuffers(SwpLoc::swapChain()->size());
        }

        void createImguiFramebuffers() {
            VkImageView attachment[1];

            VkFramebufferCreateInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            info.renderPass = imguiRenderPass;
            info.attachmentCount = 1;
            info.pAttachments = attachment;
            info.width = SwpLoc::swapChain()->width();
            info.height = SwpLoc::swapChain()->height();
            info.layers = 1;

            imguiFramebuffers.resize(SwpLoc::swapChain()->size());
            for (uint32_t i = 0; i < SwpLoc::swapChain()->size(); i++)
            {
                attachment[0] = SwpLoc::swapChain()->view(i);
                DevLoc::device()->create(info, imguiFramebuffers[i]);
            }
        }

        void createImguiRenderPass() {
            VkAttachmentDescription attachment = {};
            attachment.format = SwpLoc::swapChain()->format();
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
            DevLoc::device()->create(info, imguiRenderPass);
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
            DevLoc::device()->create(info, imguiDescriptorPool);
        }

        static void checkVKresult(VkResult err)
        {
            if (err == 0) return;
            printf("VkResult %d\n", err);
        }

        void createSwapChainRenderPass() {
            VkAttachmentDescription colorAttachment = {};
            colorAttachment.format = SwpLoc::swapChain()->format();
            colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
            colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            VkAttachmentDescription depthAttachment = {};
            depthAttachment.format = SwpLoc::swapChain()->findDepthFormat();
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

            DevLoc::device()->create(layoutInfo, descriptorSetLayout);

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
            viewport.width = (float) SwpLoc::swapChain()->width();
            viewport.height = (float) SwpLoc::swapChain()->height();
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;

            VkRect2D scissor = {};
            scissor.offset = {0, 0};
            scissor.extent = SwpLoc::swapChain()->extent();

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

            DevLoc::device()->create(pipelineLayoutInfo, pipelineLayout);

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

            DevLoc::device()->create(pipelineInfo, skyboxPipeline);

            shaderStages[0] = vertBase.info();
            shaderStages[1] = fragBase.info();

            depthStencil.depthWriteEnable = VK_TRUE;
            depthStencil.depthTestEnable = VK_TRUE;

            DevLoc::device()->create(pipelineInfo, graphicsPipeline);

            for (auto& mesh: meshes)
                if (mesh.tag != "Skybox")
                    mesh.pipeline = &graphicsPipeline;
                else
                    meshes[0].pipeline = &skyboxPipeline;
        }

        void createSwapChainFramebuffers() {
            swapChainFramebuffers.resize(SwpLoc::swapChain()->size());

            for (size_t i = 0; i < SwpLoc::swapChain()->size(); i++) {
                std::array<VkImageView, 2> attachments = {
                    SwpLoc::swapChain()->view(i),
                    SwpLoc::swapChain()->depthView()
                };

                VkFramebufferCreateInfo framebufferInfo = {};
                framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
                framebufferInfo.renderPass = renderPass;
                framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
                framebufferInfo.pAttachments = attachments.data();
                framebufferInfo.width = SwpLoc::swapChain()->width();
                framebufferInfo.height = SwpLoc::swapChain()->height();
                framebufferInfo.layers = 1;

                DevLoc::device()->create(framebufferInfo, swapChainFramebuffers[i]);
            }
        }

        bool hasStencilComponent(VkFormat format) {
            return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
        }

        void createUniformBuffers() {
            VkDeviceSize bufferSize = sizeof(UniformBufferObject);

            for (auto& mesh: meshes) {
                mesh.uniform.buffers.resize(SwpLoc::swapChain()->size());
                mesh.uniform.memory.resize(SwpLoc::swapChain()->size());

                for (size_t i = 0; i < SwpLoc::swapChain()->size(); i++) {
                    create::buffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                            | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, mesh.uniform.buffers[i], mesh.uniform.memory[i]);
                }
            }
        }

        void createDescriptorPool() {
            std::array<VkDescriptorPoolSize, 2> poolSizes = {};
            poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            poolSizes[0].descriptorCount = static_cast<uint32_t>(SwpLoc::swapChain()->size() * meshes.size());
            poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            poolSizes[1].descriptorCount = static_cast<uint32_t>(SwpLoc::swapChain()->size() * meshes.size());

            VkDescriptorPoolCreateInfo poolInfo = {};
            poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
            poolInfo.pPoolSizes = poolSizes.data();
            poolInfo.maxSets = static_cast<uint32_t>(SwpLoc::swapChain()->size() * meshes.size());

            DevLoc::device()->create(poolInfo, descriptorPool);
        }

        void allocateDescriptorSets() {
            for (auto& mesh: meshes) {
                std::vector<VkDescriptorSetLayout> layouts(SwpLoc::swapChain()->size(), *mesh.descriptor.layout);

                VkDescriptorSetAllocateInfo allocInfo = {};
                allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
                allocInfo.descriptorPool = descriptorPool;
                allocInfo.descriptorSetCount = static_cast<uint32_t>(SwpLoc::swapChain()->size());
                allocInfo.pSetLayouts = layouts.data();

                mesh.descriptor.sets.resize(SwpLoc::swapChain()->size());
                DevLoc::device()->allocate(allocInfo, mesh.descriptor.sets.data());
            }
        }

        void bindUnisToDescriptorSets() {
            for (size_t i = 0; i < SwpLoc::swapChain()->size(); i++) {
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

                    DevLoc::device()->update(static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data());
                }
            }
        }

        void recordCommandBuffers() {
            for (size_t i = 0; i < SwpLoc::swapChain()->size(); i++) {
                VkCommandBufferBeginInfo beginInfo = {};
                beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

                if (vkBeginCommandBuffer(CmdLoc::cmd()->get(i), &beginInfo) != VK_SUCCESS) {
                    throw std::runtime_error("failed to begin recording command buffer!");
                }

                VkRenderPassBeginInfo renderPassInfo = {};
                renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
                renderPassInfo.renderPass = renderPass;
                renderPassInfo.framebuffer = swapChainFramebuffers[i];
                renderPassInfo.renderArea.offset = {0, 0};
                renderPassInfo.renderArea.extent = SwpLoc::swapChain()->extent();

                std::array<VkClearValue, 2> clearValues = {};
                clearValues[0].color = {0.0f, 0.0f, 0.0f, 1.0f};
                clearValues[1].depthStencil = {1.0f, 0};

                renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
                renderPassInfo.pClearValues = clearValues.data();

                vkCmdBeginRenderPass(CmdLoc::cmd()->get(i), &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

                VkDeviceSize offsets[] = {0};

                for (auto& mesh: meshes) {
                    vkCmdBindDescriptorSets(CmdLoc::cmd()->get(i), VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipelineLayout, 0, 1, &mesh.descriptor.sets[i], 0, nullptr);
                    vkCmdBindVertexBuffers(CmdLoc::cmd()->get(i), 0, 1, &vertexBuffer, offsets);
                    vkCmdBindIndexBuffer(CmdLoc::cmd()->get(i), indexBuffer, 0, VK_INDEX_TYPE_UINT32);
                    vkCmdBindPipeline(CmdLoc::cmd()->get(i), VK_PIPELINE_BIND_POINT_GRAPHICS, *mesh.pipeline);
                    vkCmdDrawIndexed(CmdLoc::cmd()->get(i), mesh.vertex.size, 1, 0, mesh.vertex.start, 0);
                }

                vkCmdEndRenderPass(CmdLoc::cmd()->get(i));

                if (vkEndCommandBuffer(CmdLoc::cmd()->get(i)) != VK_SUCCESS) {
                    throw std::runtime_error("failed to record command buffer!");
                }
            }
        }

        void imguiRecordCommandBuffer(uint32_t imageIndex) {
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
            beginInfo.renderArea.extent.width = SwpLoc::swapChain()->width();
            beginInfo.renderArea.extent.height = SwpLoc::swapChain()->height();

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

        void createSyncObjects() {
            imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
            renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
            inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
            imagesInFlight.resize(SwpLoc::swapChain()->size(), VK_NULL_HANDLE);

            VkSemaphoreCreateInfo semaphoreInfo = {};
            semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

            VkFenceCreateInfo fenceInfo = {};
            fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

            for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                DevLoc::device()->create(semaphoreInfo, imageAvailableSemaphores[i]);
                DevLoc::device()->create(semaphoreInfo, renderFinishedSemaphores[i]);
                DevLoc::device()->create(fenceInfo, inFlightFences[i]);
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
                DevLoc::device()->map(mesh.uniform.memory[currentImage], sizeof(ubo), data);
                memcpy(data, &ubo, sizeof(ubo));
                DevLoc::device()->unmap(mesh.uniform.memory[currentImage]);
            }
        }

        void drawFrame() {
            // IMGUI
            ImGui_ImplVulkan_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            ImGui::Text("Skybox rotation");
            ImGui::Render();

            DevLoc::device()->waitFence(inFlightFences[currentFrame]);

            uint32_t imageIndex;
            VkResult result = DevLoc::device()->nextImage(SwpLoc::swapChain()->get(), imageAvailableSemaphores[currentFrame], imageIndex);

            if (result == VK_ERROR_OUT_OF_DATE_KHR) {
                recreateSwapChain();
                ImGui_ImplVulkan_SetMinImageCount(SwpLoc::swapChain()->size());
                return;
            } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
                throw std::runtime_error("failed to acquire swap chain image!");
            }

            updateUniformBuffer(imageIndex);
            camera->processInput();

            // Sync
            if (imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
                DevLoc::device()->waitFence(imagesInFlight[imageIndex]);
            }
            imagesInFlight[imageIndex] = inFlightFences[currentFrame];

            // Render IMGUI
            imguiRecordCommandBuffer(imageIndex);

            // Submit
            std::array<VkCommandBuffer, 2> submitCommandBuffers = {
                CmdLoc::cmd()->get(imageIndex),
                imguicmd->get(imageIndex)
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

            DevLoc::device()->reset(inFlightFences[currentFrame]);

            DevLoc::device()->submit(submitInfo, inFlightFences[currentFrame]);

            // Present Frame
            VkPresentInfoKHR presentInfo = {};
            presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

            presentInfo.waitSemaphoreCount = 1;
            presentInfo.pWaitSemaphores = signalSemaphores;

            VkSwapchainKHR swapChains[] = {SwpLoc::swapChain()->get()};
            presentInfo.swapchainCount = 1;
            presentInfo.pSwapchains = swapChains;

            presentInfo.pImageIndices = &imageIndex;

            result = DevLoc::device()->present(presentInfo);

            if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
                framebufferResized = false;
                recreateSwapChain();
                ImGui_ImplVulkan_SetMinImageCount(SwpLoc::swapChain()->size());
            } else if (result != VK_SUCCESS) {
                throw std::runtime_error("failed to present swap chain image!");
            }

            currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
        }
};
