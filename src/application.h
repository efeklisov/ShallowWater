#pragma once

#include <volk.h>

#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define PI glm::pi<float>()

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
#include "cubemap.h"
#include "device.h"
#include "imguiimpl.h"
#include "instance.h"
#include "locator.h"
#include "read.h"
#include "render.h"
#include "surface.h"
#include "swapchain.h"
#include "texture.h"
#include "vertex.h"
#include "descriptor.h"

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
    alignas(16) glm::mat4 invertView;
    alignas(16) glm::mat4 invertModel;
    alignas(16) glm::vec3 cameraPos;
};

struct PushConstants {
    alignas(4) glm::vec4 clipPlane = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
    alignas(4) glm::vec3 lightSource = glm::vec3(0.0f, 6.0f, -3.0f);
    alignas(4) glm::vec3 lightColor = glm::vec3(1.0f, 1.0f, 1.0f);
    alignas(4) glm::vec3 invert = glm::vec3(0.0f);
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

    Render* water;
    Render* refraction;
    Render* reflection;

    size_t currentFrame = 0;
    float currentTime = 0.0f;
    float deltaTime = 0.0f;
    float lastFrame = 0.0f;

    uint32_t nbFrames = 0.0f;
    float lastTime = 0.0f;

    Camera* camera;
    Descriptor* desc;

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;

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

        desc = new Descriptor();
        desc->addLayout({{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT}, {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT}});
        desc->addLayout({{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT}});

        desc->addPipeLayout({0}, {{VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants)}});
        desc->addPipeLayout({0, 1});

        desc->addMesh("Skybox", {0}, "models/cube.obj", new CubeMap("textures/storforsen"));
        desc->addMesh("Chalet", {0}, "models/chalet.obj", new Texture("textures/chalet.jpg"), {4.3f, 1.8f, 4.8f}, {-PI / 2, 0.0f, 0.0f});
        desc->addMesh("Lake", {0}, "models/lake.obj", new Texture("textures/lake.png"));
        desc->addMesh("Quad", {0, 1}, {10.0f, 8.5f}, {0.0f, 1.0f, -0.5f}, {PI / 2, 0.0f, 0.0f});
        desc->allocate();

        /**/ hw::loc::cmd()->vertexBuffer(vertices, vertexBuffer, vertexBufferMemory);
        /**/ hw::loc::cmd()->indexBuffer(indices, indexBuffer, indexBufferMemory);

        water = new Render("water");
        refraction = new Render("refraction", VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        reflection = new Render("refraction", VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        water->setToDefaultFBO();
        reflection->initFBO();
        refraction->initFBO();

        #pragma omp parallel for
        for (auto& render: {water, refraction, reflection}) {
            render->addPipeline(desc->pipeLayout(0), "shaders/base.vert.spv", "shaders/base.frag.spv");
            render->addPipeline(desc->pipeLayout(0), "shaders/skybox.vert.spv", "shaders/skybox.frag.spv", false);
            render->addPipeline(desc->pipeLayout(0), "shaders/lighting.vert.spv", "shaders/lighting.frag.spv");

            if (render->tag == "water") {
                render->addPipeline(desc->pipeLayout(1), "shaders/quad.vert.spv", "shaders/quad.frag.spv");
            }
        }

        createUniformBuffers();
        bindUnisToDescriptorSets();

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
        for (auto& render: {water, refraction, reflection}) {
            delete render;
        }

        imgui->cleanup();
        delete hw::loc::swapChain();

        desc->freePool();
    }

    void cleanup()
    {
        cleanupSwapChain();

        delete desc;

        hw::loc::device()->destroy(indexBuffer);
        hw::loc::device()->free(indexBufferMemory);

        hw::loc::device()->destroy(vertexBuffer);
        hw::loc::device()->free(vertexBufferMemory);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            hw::loc::device()->destroy(renderFinishedSemaphores[i]);
            hw::loc::device()->destroy(imageAvailableSemaphores[i]);
            hw::loc::device()->destroy(inFlightFences[i]);
        }

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

        water = new Render("water");
        refraction = new Render("refraction", VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        reflection = new Render("refraction", VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        water->setToDefaultFBO();
        reflection->initFBO();
        refraction->initFBO();

        #pragma omp parallel for
        for (auto& render: {water, refraction, reflection}) {
            render->addPipeline(desc->pipeLayout(0), "shaders/base.vert.spv", "shaders/base.frag.spv");
            render->addPipeline(desc->pipeLayout(0), "shaders/skybox.vert.spv", "shaders/skybox.frag.spv", false);
            render->addPipeline(desc->pipeLayout(0), "shaders/lighting.vert.spv", "shaders/lighting.frag.spv");

            if (render->tag == "water") {
                render->addPipeline(desc->pipeLayout(1), "shaders/quad.vert.spv", "shaders/quad.frag.spv");
            }
        }
        desc->allocate();

        createUniformBuffers();
        bindUnisToDescriptorSets();

        imgui->adjust();

        recordWaterCommandBuffers();
        recordRefractionCommandBuffers();
        recordReflectionCommandBuffers();
    }

    void createUniformBuffers()
    {
        VkDeviceSize bufferSize = sizeof(UniformBufferObject);

        #pragma omp parallel for
        for (auto& mesh : desc->meshes) {
            for (size_t i = 0; i < hw::loc::swapChain()->size(); i++) {
                create::buffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, desc->getUniBuffer(mesh, i, 0), desc->getUniMemory(mesh, i, 0));
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

            VkDescriptorImageInfo imageInfo2 = {};
            imageInfo2.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            std::array<VkWriteDescriptorSet, 3> descriptorWrites = {};

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

            descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[2].dstBinding = 0;
            descriptorWrites[2].dstArrayElement = 0;
            descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrites[2].descriptorCount = 1;
            descriptorWrites[2].pImageInfo = &imageInfo2;

            #pragma omp parallel for
            for (auto& mesh : desc->meshes) {
                descriptorWrites[0].dstSet = desc->getDescriptor(mesh, i, 0);
                descriptorWrites[1].dstSet = desc->getDescriptor(mesh, i, 0);

                bufferInfo.buffer = desc->getUniBuffer(mesh, i, 0);

                if (mesh->tag == "Quad") {
                    descriptorWrites[2].dstSet = desc->getDescriptor(mesh, i, 1);

                    imageInfo.imageView = refraction->colorView(i);
                    imageInfo.sampler = refraction->colorSampler(i);
                    imageInfo2.imageView = reflection->colorView(i);
                    imageInfo2.sampler = reflection->colorSampler(i);
                    hw::loc::device()->update(static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data());
                } else {
                    imageInfo.imageView = mesh->texture->view();
                    imageInfo.sampler = mesh->texture->sampler();
                    hw::loc::device()->update(static_cast<uint32_t>(descriptorWrites.size() - 1), descriptorWrites.data());
                }
            }
        }
    }

    void recordWaterCommandBuffers()
    {
        for (uint32_t i = 0; i < hw::loc::swapChain()->size(); i++) {
            VkCommandBufferBeginInfo beginInfo = {};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

            if (vkBeginCommandBuffer(water->commandBuffer(i), &beginInfo) != VK_SUCCESS) {
                throw std::runtime_error("failed to begin recording command buffer!");
            }

            VkRenderPassBeginInfo renderPassInfo = {};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass = water->renderPass();
            renderPassInfo.framebuffer = hw::loc::swapChain()->frameBuffer(i);
            renderPassInfo.renderArea.offset = { 0, 0 };
            renderPassInfo.renderArea.extent = hw::loc::swapChain()->extent();

            std::array<VkClearValue, 2> clearValues = {};
            clearValues[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
            clearValues[1].depthStencil = { 1.0f, 0 };

            renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
            renderPassInfo.pClearValues = clearValues.data();

            vkCmdBeginRenderPass(water->commandBuffer(i), &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

            VkDeviceSize offsets[] = { 0 };

            PushConstants pushConstants;

            #pragma omp parallel for
            for (auto& mesh : desc->meshes) {
                vkCmdPushConstants(water->commandBuffer(i), desc->pipeLayout(0), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &pushConstants);

                if (mesh->tag != "Quad") {
                    std::array<VkDescriptorSet, 1> sets = {desc->getDescriptor(mesh, i, 0)};
                    vkCmdBindDescriptorSets(water->commandBuffer(i), VK_PIPELINE_BIND_POINT_GRAPHICS,
                        desc->pipeLayout(0), 0, sets.size(), sets.data(), 0, nullptr);
                } else {
                    std::array<VkDescriptorSet, 2> sets = {desc->getDescriptor(mesh, i, 0), desc->getDescriptor(mesh, i, 1)};
                    vkCmdBindDescriptorSets(water->commandBuffer(i), VK_PIPELINE_BIND_POINT_GRAPHICS,
                        desc->pipeLayout(1), 0, sets.size(), sets.data(), 0, nullptr);
                }

                vkCmdBindVertexBuffers(water->commandBuffer(i), 0, 1, &vertexBuffer, offsets);
                vkCmdBindIndexBuffer(water->commandBuffer(i), indexBuffer, 0, VK_INDEX_TYPE_UINT32);

                if (mesh->tag == "Chalet")
                    vkCmdBindPipeline(water->commandBuffer(i), VK_PIPELINE_BIND_POINT_GRAPHICS, water->pipeline(0));
                else if (mesh->tag == "Skybox")
                    vkCmdBindPipeline(water->commandBuffer(i), VK_PIPELINE_BIND_POINT_GRAPHICS, water->pipeline(1));
                else if (mesh->tag == "Quad")
                    vkCmdBindPipeline(water->commandBuffer(i), VK_PIPELINE_BIND_POINT_GRAPHICS, water->pipeline(3));
                else
                    vkCmdBindPipeline(water->commandBuffer(i), VK_PIPELINE_BIND_POINT_GRAPHICS, water->pipeline(2));

                vkCmdDrawIndexed(water->commandBuffer(i), mesh->vertex.size, 1, 0, mesh->vertex.start, 0);
            }

            vkCmdEndRenderPass(water->commandBuffer(i));

            if (vkEndCommandBuffer(water->commandBuffer(i)) != VK_SUCCESS) {
                throw std::runtime_error("failed to record command buffer!");
            }
        }
    }

    void recordRefractionCommandBuffers()
    {
        for (uint32_t i = 0; i < hw::loc::swapChain()->size(); i++) {
            VkCommandBufferBeginInfo beginInfo = {};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

            if (vkBeginCommandBuffer(refraction->commandBuffer(i), &beginInfo) != VK_SUCCESS) {
                throw std::runtime_error("failed to begin recording command buffer!");
            }

            VkRenderPassBeginInfo renderPassInfo = {};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass = refraction->renderPass();
            renderPassInfo.framebuffer = refraction->frameBuffer(i);
            renderPassInfo.renderArea.offset = { 0, 0 };
            renderPassInfo.renderArea.extent = hw::loc::swapChain()->extent();

            std::array<VkClearValue, 2> clearValues = {};
            clearValues[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
            clearValues[1].depthStencil = { 1.0f, 0 };

            renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
            renderPassInfo.pClearValues = clearValues.data();

            vkCmdBeginRenderPass(refraction->commandBuffer(i), &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

            VkDeviceSize offsets[] = { 0 };

            PushConstants pushConstants;
            pushConstants.clipPlane = glm::vec4(0.0f, -1.0f, 0.0f, 1.0f);

            #pragma omp parallel for
            for (auto& mesh : desc->meshes) {
                if (mesh->tag == "Quad")
                    continue;

                vkCmdPushConstants(refraction->commandBuffer(i), desc->pipeLayout(0), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &pushConstants);

                std::array<VkDescriptorSet, 1> sets = {desc->getDescriptor(mesh, i, 0)};
                vkCmdBindDescriptorSets(refraction->commandBuffer(i), VK_PIPELINE_BIND_POINT_GRAPHICS,
                    desc->pipeLayout(0), 0, sets.size(), sets.data(), 0, nullptr);

                vkCmdBindVertexBuffers(refraction->commandBuffer(i), 0, 1, &vertexBuffer, offsets);
                vkCmdBindIndexBuffer(refraction->commandBuffer(i), indexBuffer, 0, VK_INDEX_TYPE_UINT32);

                if (mesh->tag == "Chalet")
                    vkCmdBindPipeline(refraction->commandBuffer(i), VK_PIPELINE_BIND_POINT_GRAPHICS, refraction->pipeline(0));
                else if (mesh->tag == "Skybox")
                    vkCmdBindPipeline(refraction->commandBuffer(i), VK_PIPELINE_BIND_POINT_GRAPHICS, refraction->pipeline(1));
                else
                    vkCmdBindPipeline(refraction->commandBuffer(i), VK_PIPELINE_BIND_POINT_GRAPHICS, refraction->pipeline(2));

                vkCmdDrawIndexed(refraction->commandBuffer(i), mesh->vertex.size, 1, 0, mesh->vertex.start, 0);
            }

            vkCmdEndRenderPass(refraction->commandBuffer(i));

            if (vkEndCommandBuffer(refraction->commandBuffer(i)) != VK_SUCCESS) {
                throw std::runtime_error("failed to record command buffer!");
            }
        }
    }

    void recordReflectionCommandBuffers()
    {
        for (uint32_t i = 0; i < hw::loc::swapChain()->size(); i++) {
            VkCommandBufferBeginInfo beginInfo = {};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

            if (vkBeginCommandBuffer(reflection->commandBuffer(i), &beginInfo) != VK_SUCCESS) {
                throw std::runtime_error("failed to begin recording command buffer!");
            }

            VkRenderPassBeginInfo renderPassInfo = {};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass = reflection->renderPass();
            renderPassInfo.framebuffer = reflection->frameBuffer(i);
            renderPassInfo.renderArea.offset = { 0, 0 };
            renderPassInfo.renderArea.extent = hw::loc::swapChain()->extent();

            std::array<VkClearValue, 2> clearValues = {};
            clearValues[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
            clearValues[1].depthStencil = { 1.0f, 0 };

            renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
            renderPassInfo.pClearValues = clearValues.data();

            vkCmdBeginRenderPass(reflection->commandBuffer(i), &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

            VkDeviceSize offsets[] = { 0 };

            PushConstants pushConstants;
            pushConstants.clipPlane = glm::vec4(0.0f, 1.0f, 0.0f, -1.0f);
            pushConstants.invert = glm::vec3(1.0f);

            #pragma omp parallel for
            for (auto& mesh : desc->meshes) {
                if (mesh->tag == "Quad")
                    continue;

                vkCmdPushConstants(reflection->commandBuffer(i), desc->pipeLayout(0), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &pushConstants);

                std::array<VkDescriptorSet, 1> sets = {desc->getDescriptor(mesh, i, 0)};
                vkCmdBindDescriptorSets(reflection->commandBuffer(i), VK_PIPELINE_BIND_POINT_GRAPHICS,
                    desc->pipeLayout(0), 0, sets.size(), sets.data(), 0, nullptr);

                vkCmdBindVertexBuffers(reflection->commandBuffer(i), 0, 1, &vertexBuffer, offsets);
                vkCmdBindIndexBuffer(reflection->commandBuffer(i), indexBuffer, 0, VK_INDEX_TYPE_UINT32);

                if (mesh->tag == "Chalet")
                    vkCmdBindPipeline(reflection->commandBuffer(i), VK_PIPELINE_BIND_POINT_GRAPHICS, reflection->pipeline(0));
                else if (mesh->tag == "Skybox")
                    vkCmdBindPipeline(reflection->commandBuffer(i), VK_PIPELINE_BIND_POINT_GRAPHICS, reflection->pipeline(1));
                else
                    vkCmdBindPipeline(reflection->commandBuffer(i), VK_PIPELINE_BIND_POINT_GRAPHICS, reflection->pipeline(2));

                vkCmdDrawIndexed(reflection->commandBuffer(i), mesh->vertex.size, 1, 0, mesh->vertex.start, 0);
            }

            vkCmdEndRenderPass(reflection->commandBuffer(i));

            if (vkEndCommandBuffer(reflection->commandBuffer(i)) != VK_SUCCESS) {
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

        #pragma omp parallel for
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
        ubo.invertView = camera->viewI;
        ubo.cameraPos = camera->cameraPos;

        #pragma omp parallel for
        for (auto& mesh : desc->meshes) {
            glm::mat4 position;
            glm::mat4 rotation = glm::mat4_cast(glm::normalize(glm::quat(mesh->rotation)));
            glm::mat4 scale;

            if (mesh->tag != "Skybox") {
                position = glm::translate(glm::mat4(1.0f), mesh->transform);

                glm::vec3 _scale = mesh->scale;
                _scale.x *= -1;
                scale = glm::scale(glm::mat4(1.0f), _scale);
            } else {
                position = glm::translate(glm::mat4(1.0f), camera->cameraPos);
                scale = glm::scale(glm::mat4(1.0f), mesh->scale);
            }

            ubo.model = position * rotation * scale * glm::mat4(1.0f);
            if (mesh->tag == "Skybox")
                ubo.invertModel = glm::translate(glm::mat4(1.0f), camera->cameraPos - camera->distance(camera->cameraPos)) * rotation * scale * glm::mat4(1.0f);
            else ubo.invertModel = ubo.model;

            void* data;
            hw::loc::device()->map(desc->getUniMemory(mesh, currentImage, 0), sizeof(ubo), data);
            memcpy(data, &ubo, sizeof(ubo));
            hw::loc::device()->unmap(desc->getUniMemory(mesh, currentImage, 0));
        }
    }

    void drawFrame()
    {
        // IMGUI
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        /* ImGui::Text("Clip Distance Change"); */
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
            refraction->commandBuffer(imageIndex),
            reflection->commandBuffer(imageIndex),
            water->commandBuffer(imageIndex),
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
