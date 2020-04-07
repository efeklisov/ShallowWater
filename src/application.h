#pragma once

#include <volk.h>

#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <optional>
#include <set>
#include <stdexcept>
#include <vector>

#include "camera.h"
#include "command.h"
#include "create.h"
#include "cubemap.h"
#include "device.h"
#include "imguiimpl.h"
#include "instance.h"
#include "locator.h"
#include "mesh.h"
#include "read.h"
#include "render.h"
#include "shader.h"
#include "surface.h"
#include "swapchain.h"
#include "texture.h"
#include "vertex.h"

const int WIDTH = 1280;
const int HEIGHT = 768;

const int MAX_FRAMES_IN_FLIGHT = 3;

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

struct UniformBufferObject {
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
    alignas(16) glm::vec3 cameraPos;
};

struct PushConstants {
    alignas(4) glm::vec4 clipPlane = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
    alignas(4) glm::vec3 lightSource = glm::vec3(0.0f, 6.0f, -3.0f);
    alignas(4) glm::vec3 lightColor = glm::vec3(1.0f, 1.0f, 1.0f);
};

class Application {
public:
    void run()
    {
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

    struct StructRender {
        struct StructRefraction {
            VkRenderPass pass;

            struct StructPipeline {
                VkPipeline base;
                VkPipeline lighting;
                VkPipeline skybox;
            } pipeline;

            render::FBO* frameBuffer;
            std::vector<VkCommandBuffer> commandBuffers;
        } refraction;

        struct StructReflection {
            VkRenderPass pass;

            struct StructPipeline {
                VkPipeline base;
                VkPipeline lighting;
                VkPipeline skybox;
            } pipeline;

            render::FBO* frameBuffer;
            std::vector<VkCommandBuffer> commandBuffers;
        } reflection;

        struct StructWater {
            VkRenderPass pass;

            struct StructPipeline {
                VkPipeline base;
                VkPipeline lighting;
                VkPipeline skybox;
            } pipeline;

            std::vector<VkCommandBuffer> commandBuffers;
        } water;
    } render;

    VkDescriptorSetLayout descriptorSetLayout;

    size_t currentFrame = 0;
    float currentTime = 0.0f;
    float deltaTime = 0.0f;
    float lastFrame = 0.0f;

    uint32_t nbFrames = 0.0f;
    float lastTime = 0.0f;

    Camera* camera;

    VkPipelineLayout pipelineLayout;

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

    bool framebufferResized = false;

    void initWindow()
    {
        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        window = glfwCreateWindow(WIDTH, HEIGHT, "Engine", nullptr, nullptr);
        glfwSetWindowUserPointer(window, this);
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

        glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
        glfwSetCursorPosCallback(window, mouse_callback);
    }

    static void framebufferResizeCallback(GLFWwindow* window, int width, int height)
    {
        auto app = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
        app->framebufferResized = true;
    }

    static void mouse_callback(GLFWwindow* window, double xpos, double ypos)
    {
        auto app = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));

        app->camera->processMouse(xpos, ypos);
    }

    void showFPS()
    {
        // Measure speed
        double currentTime = glfwGetTime();
        double delta = currentTime - lastTime;
        nbFrames++;
        if (delta >= 1.0) {
            double frameTime = 1000.0 / double(nbFrames);
            double fps = double(nbFrames) / delta;

            std::stringstream ss;
            ss << "Engine : " << fps << " FPS, " << frameTime << " ms";

            glfwSetWindowTitle(window, ss.str().c_str());

            nbFrames = 0;
            lastTime = currentTime;
        }
    }

    void initVulkan()
    {
        hw::loc::provide(new hw::Instance(enableValidationLayers));
        hw::loc::provide(new hw::Surface(window));
        hw::loc::provide(new hw::Device(enableValidationLayers));
        hw::loc::provide(new hw::Command());
        hw::loc::provide(new hw::SwapChain(window));

        hw::loc::provide(vertices);
        hw::loc::provide(indices);

        /**/ meshes.push_back(Mesh("Skybox", "models/cube.obj", std::make_unique<CubeMap>("textures/storforsen")));
        /**/ meshes.push_back(Mesh("Chalet", "models/chalet.obj", std::make_unique<Texture>("textures/chalet.jpg"),
            glm::vec3(4.3177f, 1.8368f, 4.7955f), glm::vec3(-glm::pi<float>() / 2, 0.0f, 0.0f)));
        /**/ meshes.push_back(Mesh("Lake", "models/lake.obj", std::make_unique<Texture>("textures/lake.png")));
        meshes.push_back(Mesh("Quad1", glm::vec2(6.0f, 4.0f), glm::vec3(4.0f, 6.0f, 0.0f)));
        meshes.push_back(Mesh("Quad2", glm::vec2(6.0f, 4.0f), glm::vec3(-4.0f, 6.0f, 0.0f)));

        /**/ hw::loc::cmd()->vertexBuffer(vertices, vertexBuffer, vertexBufferMemory);
        /**/ hw::loc::cmd()->indexBuffer(indices, indexBuffer, indexBufferMemory);

        /**/ createLayouts();

        render::pass(render.refraction.pass, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        render::pass(render.reflection.pass, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        render::pass(render.water.pass);

        render::defaultFBO(render.water.pass);
        render.refraction.frameBuffer = new render::FBO(&render.refraction.pass);
        render.reflection.frameBuffer = new render::FBO(&render.reflection.pass);

        render::pipeline(render.water.pipeline.base, render.water.pass, pipelineLayout, "shaders/base.vert.spv", "shaders/base.frag.spv");
        render::pipeline(render.water.pipeline.lighting, render.water.pass, pipelineLayout, "shaders/lighting.vert.spv", "shaders/lighting.frag.spv");
        render::pipeline(render.water.pipeline.skybox, render.water.pass, pipelineLayout, "shaders/skybox.vert.spv", "shaders/skybox.frag.spv", false);

        render::pipeline(render.refraction.pipeline.base, render.refraction.pass, pipelineLayout, "shaders/base.vert.spv", "shaders/base.frag.spv");
        render::pipeline(render.refraction.pipeline.lighting, render.refraction.pass, pipelineLayout, "shaders/lighting.vert.spv", "shaders/lighting.frag.spv");
        render::pipeline(render.refraction.pipeline.skybox, render.refraction.pass, pipelineLayout, "shaders/skybox.vert.spv", "shaders/skybox.frag.spv", false);

        render::pipeline(render.reflection.pipeline.base, render.reflection.pass, pipelineLayout, "shaders/base.vert.spv", "shaders/base.frag.spv");
        render::pipeline(render.reflection.pipeline.lighting, render.reflection.pass, pipelineLayout, "shaders/lighting.vert.spv", "shaders/lighting.frag.spv");
        render::pipeline(render.reflection.pipeline.skybox, render.reflection.pass, pipelineLayout, "shaders/skybox.vert.spv", "shaders/skybox.frag.spv", false);

        createDescriptorPool();
        allocateDescriptorSets();
        createUniformBuffers();
        bindUnisToDescriptorSets();

        hw::loc::cmd()->createCommandBuffers(render.water.commandBuffers, hw::loc::swapChain()->size());
        hw::loc::cmd()->createCommandBuffers(render.refraction.commandBuffers, hw::loc::swapChain()->size());
        hw::loc::cmd()->createCommandBuffers(render.reflection.commandBuffers, hw::loc::swapChain()->size());

        recordWaterCommandBuffers();
        recordRefractionCommandBuffers();
        recordReflectionCommandBuffers();

        /**/ createSyncObjects();
    }

    void mainLoop()
    {
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();

            currentTime = glfwGetTime();
            deltaTime = currentTime - lastFrame;
            lastFrame = currentTime;
            showFPS();

            camera->update(deltaTime);

            drawFrame();
        }

        hw::loc::device()->waitDevice();
    }

    void cleanupSwapChain()
    {
        hw::loc::cmd()->freeCommandBuffers(render.reflection.commandBuffers);
        hw::loc::cmd()->freeCommandBuffers(render.refraction.commandBuffers);
        hw::loc::cmd()->freeCommandBuffers(render.water.commandBuffers);

        hw::loc::device()->destroy(render.reflection.pipeline.skybox);
        hw::loc::device()->destroy(render.reflection.pipeline.lighting);
        hw::loc::device()->destroy(render.reflection.pipeline.base);

        hw::loc::device()->destroy(render.refraction.pipeline.skybox);
        hw::loc::device()->destroy(render.refraction.pipeline.lighting);
        hw::loc::device()->destroy(render.refraction.pipeline.base);

        hw::loc::device()->destroy(render.water.pipeline.skybox);
        hw::loc::device()->destroy(render.water.pipeline.lighting);
        hw::loc::device()->destroy(render.water.pipeline.base);

        delete render.refraction.frameBuffer;
        delete render.reflection.frameBuffer;

        hw::loc::device()->destroy(render.reflection.pass);
        hw::loc::device()->destroy(render.refraction.pass);
        hw::loc::device()->destroy(render.water.pass);

        imgui->cleanup();
        delete hw::loc::swapChain();

        for (auto& mesh : meshes)
            mesh.free();

        hw::loc::device()->destroy(descriptorPool);
    }

    void cleanup()
    {
        cleanupSwapChain();

        hw::loc::device()->destroy(descriptorSetLayout);
        hw::loc::device()->destroy(pipelineLayout);

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
        delete camera;
        delete hw::loc::cmd();
        delete hw::loc::device();
        delete hw::loc::surface();
        delete hw::loc::instance();

        glfwDestroyWindow(window);

        glfwTerminate();
    }

    void recreateSwapChain()
    {
        int width = 0, height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        while (width == 0 || height == 0) {
            glfwGetFramebufferSize(window, &width, &height);
            glfwWaitEvents();
        }

        hw::loc::device()->waitDevice();

        cleanupSwapChain();

        hw::loc::provide(new hw::SwapChain(window));

        render::pass(render.refraction.pass);
        render::pass(render.reflection.pass);
        render::pass(render.water.pass);

        render::defaultFBO(render.water.pass);
        render.refraction.frameBuffer = new render::FBO(&render.refraction.pass);
        render.reflection.frameBuffer = new render::FBO(&render.reflection.pass);

        render::pipeline(render.water.pipeline.base, render.water.pass, pipelineLayout, "shaders/base.vert.spv", "shaders/base.frag.spv");
        render::pipeline(render.water.pipeline.lighting, render.water.pass, pipelineLayout, "shaders/lighting.vert.spv", "shaders/lighting.frag.spv");
        render::pipeline(render.water.pipeline.skybox, render.water.pass, pipelineLayout, "shaders/skybox.vert.spv", "shaders/skybox.frag.spv", false);

        render::pipeline(render.refraction.pipeline.base, render.refraction.pass, pipelineLayout, "shaders/base.vert.spv", "shaders/base.frag.spv");
        render::pipeline(render.refraction.pipeline.lighting, render.refraction.pass, pipelineLayout, "shaders/lighting.vert.spv", "shaders/lighting.frag.spv");
        render::pipeline(render.refraction.pipeline.skybox, render.refraction.pass, pipelineLayout, "shaders/skybox.vert.spv", "shaders/skybox.frag.spv", false);

        render::pipeline(render.reflection.pipeline.base, render.reflection.pass, pipelineLayout, "shaders/base.vert.spv", "shaders/base.frag.spv");
        render::pipeline(render.reflection.pipeline.lighting, render.reflection.pass, pipelineLayout, "shaders/lighting.vert.spv", "shaders/lighting.frag.spv");
        render::pipeline(render.reflection.pipeline.skybox, render.reflection.pass, pipelineLayout, "shaders/skybox.vert.spv", "shaders/skybox.frag.spv", false);

        createUniformBuffers();
        createDescriptorPool();
        allocateDescriptorSets();
        bindUnisToDescriptorSets();

        imgui->adjust();

        hw::loc::cmd()->createCommandBuffers(render.water.commandBuffers, hw::loc::swapChain()->size());
        hw::loc::cmd()->createCommandBuffers(render.refraction.commandBuffers, hw::loc::swapChain()->size());
        hw::loc::cmd()->createCommandBuffers(render.reflection.commandBuffers, hw::loc::swapChain()->size());

        recordWaterCommandBuffers();
        recordRefractionCommandBuffers();
        recordReflectionCommandBuffers();
    }

    void createLayouts()
    {
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

        std::array<VkDescriptorSetLayoutBinding, 2> bindings = { uboLayoutBinding, samplerLayoutBinding };
        VkDescriptorSetLayoutCreateInfo layoutInfo = {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        hw::loc::device()->create(layoutInfo, descriptorSetLayout);

        for (auto& mesh : meshes)
            mesh.descriptor.layout = &descriptorSetLayout;

        std::vector<VkDescriptorSetLayout> layouts = {
            descriptorSetLayout
        };

        VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(layouts.size());
        pipelineLayoutInfo.pSetLayouts = layouts.data();

        VkPushConstantRange pushConstantRanges = { VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants) };

        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRanges;

        hw::loc::device()->create(pipelineLayoutInfo, pipelineLayout);
    }

    void createDescriptorPool()
    {
        std::array<VkDescriptorPoolSize, 2> poolSizes = {};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[0].descriptorCount = static_cast<uint32_t>(hw::loc::swapChain()->size() * meshes.size() * 3);
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[1].descriptorCount = static_cast<uint32_t>(hw::loc::swapChain()->size() * meshes.size() * 3);

        VkDescriptorPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = static_cast<uint32_t>(hw::loc::swapChain()->size() * meshes.size());

        hw::loc::device()->create(poolInfo, descriptorPool);
    }

    void allocateDescriptorSets()
    {
        for (auto& mesh : meshes) {
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

    void createUniformBuffers()
    {
        VkDeviceSize bufferSize = sizeof(UniformBufferObject);

        for (auto& mesh : meshes) {
            mesh.uniform.buffers.resize(hw::loc::swapChain()->size());
            mesh.uniform.memory.resize(hw::loc::swapChain()->size());

            for (size_t i = 0; i < hw::loc::swapChain()->size(); i++) {
                create::buffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, mesh.uniform.buffers[i], mesh.uniform.memory[i]);
            }
        }
    }

    void bindUnisToDescriptorSets()
    {
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

            for (auto& mesh : meshes) {
                descriptorWrites[0].dstSet = mesh.descriptor.sets[i];
                descriptorWrites[1].dstSet = mesh.descriptor.sets[i];

                bufferInfo.buffer = mesh.uniform.buffers[i];

                if (mesh.tag == "Quad1") {
                    imageInfo.imageView = render.refraction.frameBuffer->colorView(i);
                    imageInfo.sampler = render.refraction.frameBuffer->colorSampler(i);
                } else if (mesh.tag == "Quad2") {
                    imageInfo.imageView = render.reflection.frameBuffer->colorView(i);
                    imageInfo.sampler = render.reflection.frameBuffer->colorSampler(i);
                } else {
                    imageInfo.imageView = mesh.texture->view();
                    imageInfo.sampler = mesh.texture->sampler();
                }

                hw::loc::device()->update(static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data());
            }
        }
    }

    void recordWaterCommandBuffers()
    {
        for (uint32_t i = 0; i < hw::loc::swapChain()->size(); i++) {
            VkCommandBufferBeginInfo beginInfo = {};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

            if (vkBeginCommandBuffer(render.water.commandBuffers[i], &beginInfo) != VK_SUCCESS) {
                throw std::runtime_error("failed to begin recording command buffer!");
            }

            VkRenderPassBeginInfo renderPassInfo = {};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass = render.water.pass;
            renderPassInfo.framebuffer = hw::loc::swapChain()->frameBuffer(i);
            renderPassInfo.renderArea.offset = { 0, 0 };
            renderPassInfo.renderArea.extent = hw::loc::swapChain()->extent();

            std::array<VkClearValue, 2> clearValues = {};
            clearValues[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
            clearValues[1].depthStencil = { 1.0f, 0 };

            renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
            renderPassInfo.pClearValues = clearValues.data();

            vkCmdBeginRenderPass(render.water.commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

            VkDeviceSize offsets[] = { 0 };

            PushConstants pushConstants;

            for (auto& mesh : meshes) {
                vkCmdPushConstants(render.water.commandBuffers[i], pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &pushConstants);

                vkCmdBindDescriptorSets(render.water.commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS,
                    pipelineLayout, 0, 1, &mesh.descriptor.sets[i], 0, nullptr);
                vkCmdBindVertexBuffers(render.water.commandBuffers[i], 0, 1, &vertexBuffer, offsets);
                vkCmdBindIndexBuffer(render.water.commandBuffers[i], indexBuffer, 0, VK_INDEX_TYPE_UINT32);

                if (mesh.tag == "Chalet" || mesh.tag == "Quad1" || mesh.tag == "Quad2")
                    vkCmdBindPipeline(render.water.commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, render.water.pipeline.base);
                else if (mesh.tag == "Skybox")
                    vkCmdBindPipeline(render.water.commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, render.water.pipeline.skybox);
                else
                    vkCmdBindPipeline(render.water.commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, render.water.pipeline.lighting);

                vkCmdDrawIndexed(render.water.commandBuffers[i], mesh.vertex.size, 1, 0, mesh.vertex.start, 0);
            }

            vkCmdEndRenderPass(render.water.commandBuffers[i]);

            if (vkEndCommandBuffer(render.water.commandBuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to record command buffer!");
            }
        }
    }

    void recordRefractionCommandBuffers()
    {
        for (uint32_t i = 0; i < hw::loc::swapChain()->size(); i++) {
            VkCommandBufferBeginInfo beginInfo = {};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

            if (vkBeginCommandBuffer(render.refraction.commandBuffers[i], &beginInfo) != VK_SUCCESS) {
                throw std::runtime_error("failed to begin recording command buffer!");
            }

            VkRenderPassBeginInfo renderPassInfo = {};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass = render.refraction.pass;
            renderPassInfo.framebuffer = render.refraction.frameBuffer->get(i);
            renderPassInfo.renderArea.offset = { 0, 0 };
            renderPassInfo.renderArea.extent = hw::loc::swapChain()->extent();

            std::array<VkClearValue, 2> clearValues = {};
            clearValues[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
            clearValues[1].depthStencil = { 1.0f, 0 };

            renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
            renderPassInfo.pClearValues = clearValues.data();

            vkCmdBeginRenderPass(render.refraction.commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

            VkDeviceSize offsets[] = { 0 };

            PushConstants pushConstants;
            pushConstants.clipPlane = glm::vec4(0.0f, 1.0f, 0.0f, -1.0f);

            for (auto& mesh : meshes) {
                if (mesh.tag == "Quad1")
                    continue;

                if (mesh.tag == "Quad2")
                    continue;

                vkCmdPushConstants(render.refraction.commandBuffers[i], pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &pushConstants);

                vkCmdBindDescriptorSets(render.refraction.commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS,
                    pipelineLayout, 0, 1, &mesh.descriptor.sets[i], 0, nullptr);
                vkCmdBindVertexBuffers(render.refraction.commandBuffers[i], 0, 1, &vertexBuffer, offsets);
                vkCmdBindIndexBuffer(render.refraction.commandBuffers[i], indexBuffer, 0, VK_INDEX_TYPE_UINT32);

                if (mesh.tag == "Chalet")
                    vkCmdBindPipeline(render.refraction.commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, render.refraction.pipeline.base);
                else if (mesh.tag == "Skybox")
                    vkCmdBindPipeline(render.refraction.commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, render.refraction.pipeline.skybox);
                else
                    vkCmdBindPipeline(render.refraction.commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, render.refraction.pipeline.lighting);

                vkCmdDrawIndexed(render.refraction.commandBuffers[i], mesh.vertex.size, 1, 0, mesh.vertex.start, 0);
            }

            vkCmdEndRenderPass(render.refraction.commandBuffers[i]);

            if (vkEndCommandBuffer(render.refraction.commandBuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to record command buffer!");
            }
        }
    }

    void recordReflectionCommandBuffers()
    {
        for (uint32_t i = 0; i < hw::loc::swapChain()->size(); i++) {
            VkCommandBufferBeginInfo beginInfo = {};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

            if (vkBeginCommandBuffer(render.reflection.commandBuffers[i], &beginInfo) != VK_SUCCESS) {
                throw std::runtime_error("failed to begin recording command buffer!");
            }

            VkRenderPassBeginInfo renderPassInfo = {};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass = render.reflection.pass;
            renderPassInfo.framebuffer = render.reflection.frameBuffer->get(i);
            renderPassInfo.renderArea.offset = { 0, 0 };
            renderPassInfo.renderArea.extent = hw::loc::swapChain()->extent();

            std::array<VkClearValue, 2> clearValues = {};
            clearValues[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
            clearValues[1].depthStencil = { 1.0f, 0 };

            renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
            renderPassInfo.pClearValues = clearValues.data();

            vkCmdBeginRenderPass(render.reflection.commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

            VkDeviceSize offsets[] = { 0 };

            PushConstants pushConstants;
            pushConstants.clipPlane = glm::vec4(0.0f, -1.0f, 0.0f, 1.0f);

            for (auto& mesh : meshes) {
                if (mesh.tag == "Quad1")
                    continue;

                if (mesh.tag == "Quad2")
                    continue;

                vkCmdPushConstants(render.reflection.commandBuffers[i], pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &pushConstants);

                vkCmdBindDescriptorSets(render.reflection.commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS,
                    pipelineLayout, 0, 1, &mesh.descriptor.sets[i], 0, nullptr);
                vkCmdBindVertexBuffers(render.reflection.commandBuffers[i], 0, 1, &vertexBuffer, offsets);
                vkCmdBindIndexBuffer(render.reflection.commandBuffers[i], indexBuffer, 0, VK_INDEX_TYPE_UINT32);

                if (mesh.tag == "Chalet")
                    vkCmdBindPipeline(render.reflection.commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, render.reflection.pipeline.base);
                else if (mesh.tag == "Skybox")
                    vkCmdBindPipeline(render.reflection.commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, render.reflection.pipeline.skybox);
                else
                    vkCmdBindPipeline(render.reflection.commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, render.reflection.pipeline.lighting);

                vkCmdDrawIndexed(render.reflection.commandBuffers[i], mesh.vertex.size, 1, 0, mesh.vertex.start, 0);
            }

            vkCmdEndRenderPass(render.reflection.commandBuffers[i]);

            if (vkEndCommandBuffer(render.reflection.commandBuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to record command buffer!");
            }
        }
    }

    void createSyncObjects()
    {
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

    void updateUniformBuffer(uint32_t currentImage)
    {
        UniformBufferObject ubo = {};

        ubo.proj = camera->proj;
        ubo.view = camera->view;
        ubo.cameraPos = camera->cameraPos;
        for (auto& mesh : meshes) {
            glm::mat4 position;
            glm::mat4 rotation = glm::mat4_cast(glm::normalize(glm::quat(mesh.rotation)));
            glm::mat4 scale;

            if (mesh.tag != "Skybox") {
                position = glm::translate(glm::mat4(1.0f), mesh.transform);

                glm::vec3 _scale = mesh.scale;
                _scale.x *= -1;
                scale = glm::scale(glm::mat4(1.0f), _scale);
            } else {
                position = glm::translate(glm::mat4(1.0f), camera->cameraPos);
                scale = glm::scale(glm::mat4(1.0f), mesh.scale);
            }

            ubo.model = position * rotation * scale * glm::mat4(1.0f);

            void* data;
            hw::loc::device()->map(mesh.uniform.memory[currentImage], sizeof(ubo), data);
            memcpy(data, &ubo, sizeof(ubo));
            hw::loc::device()->unmap(mesh.uniform.memory[currentImage]);
        }
    }

    void drawFrame()
    {
        // IMGUI
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGui::Text("Clip Distance Change");
        /* ImGui::SliderFloat("Height", &clipPlane.w, -10.0f, 10.0f); */
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

        // Sync to GPU
        if (imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
            hw::loc::device()->waitFence(imagesInFlight[imageIndex]);
        }
        imagesInFlight[imageIndex] = inFlightFences[currentFrame];

        // Render IMGUI
        imgui->recordCommandBuffer(imageIndex);

        // Submit
        std::array<VkCommandBuffer, 4> submitCommandBuffers = {
            render.refraction.commandBuffers[imageIndex],
            render.reflection.commandBuffers[imageIndex],
            render.water.commandBuffers[imageIndex],
            imgui->getCommandBuffer(imageIndex)
        };

        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[currentFrame] };
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;

        submitInfo.commandBufferCount = static_cast<uint32_t>(submitCommandBuffers.size());
        submitInfo.pCommandBuffers = submitCommandBuffers.data();

        VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[currentFrame] };
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        hw::loc::device()->reset(inFlightFences[currentFrame]);

        hw::loc::device()->submit(submitInfo, inFlightFences[currentFrame]);

        // Present Frame
        VkPresentInfoKHR presentInfo = {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;

        VkSwapchainKHR swapChains[] = { hw::loc::swapChain()->get() };
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
