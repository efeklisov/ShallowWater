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
#include "compute.h"

const int WIDTH = 1440;
const int HEIGHT = 900;

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
    alignas(16) glm::vec4 cameraPos;
};

struct UserSimulationInput {
    alignas(16) glm::vec4 mouse;
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

    #ifdef IMGUI_ON
        imgui = new ImGuiImpl(window);
    #endif

        camera = new Camera(window, WIDTH, HEIGHT);
        mainLoop();
        cleanup();
    }

private:
    GLFWwindow* window;
    ImGuiImpl* imgui;

    Compute* comp;
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

    std::vector<VkSemaphore> computeFinishedSemaphores;
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
        hw::loc::provide(new hw::Command(VK_COMMAND_POOL_CREATE_PROTECTED_BIT, true), true);
        hw::loc::provide(new hw::SwapChain(window));

        hw::loc::provide(vertices);
        hw::loc::provide(indices);

        desc = new Descriptor();
        desc->addLayout({
                {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT}, 
                {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT}
            });
        desc->addLayout({
                {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT},
                {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_VERTEX_BIT}
            });
        desc->addLayout({
                {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT},
                {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT},
                {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT},
                {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT} 
            });

        desc->addPipeLayout({0}, {{VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants)}});
        desc->addPipeLayout({0, 1});
        desc->addPipeLayout({2});

        desc->addMesh("Skybox", {0}, "models/cube.obj", new CubeMap("textures/storforsen"));
        desc->addMesh("Chalet", {0}, "models/chalet.obj", new Texture("textures/chalet.jpg"), {4.3f, 1.8f, 4.8f}, {-PI / 2, 0.0f, 0.0f});
        desc->addMesh("Lake", {0}, "models/lake.obj", new Texture("textures/lake.png"));
        /* desc->addMesh("Quad", {0, 1}, "models/grid.obj", nullptr, {0.0f, 1.0f, -0.5f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}); */
        desc->addMesh("Quad", {0, 1}, "models/grid.obj", nullptr, {0.0f, 1.0f, -2.0f}, {0.0f, 0.0f, 0.0f}, {10.0f, 1.0f, 12.0f});
        desc->addMesh("Simulation", {2});
        desc->allocate();

        create::vertexBuffer(vertices, vertexBuffer, vertexBufferMemory);
        create::indexBuffer(indices, indexBuffer, indexBufferMemory);

        setupCompute();
        setupRender();

        createUniformBuffers();
        bindUnisToDescriptorSets();

        recordSimulationCommandBuffers();
        recordWaterCommandBuffers();
        recordRefractionCommandBuffers();
        recordReflectionCommandBuffers();

        createSyncObjects();
    }

    void setupCompute() {
        comp = new Compute("simulation", 3, 1024, 1024);

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        create::staging("textures/heightmap.jpg", stagingBuffer, stagingBufferMemory);

        for (uint32_t i = 0; i < hw::loc::swapChain()->size(); i++) {
            hw::loc::comp()->transitionImageLayout(comp->color(i, 0), VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
            hw::loc::comp()->copyBufferToImage(stagingBuffer, comp->color(i, 0), comp->extent().width, comp->extent().height);
            hw::loc::comp()->transitionImageLayout(comp->color(i, 0), VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);

            hw::loc::comp()->transitionImageLayout(comp->color(i, 1), VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
            hw::loc::comp()->copyBufferToImage(stagingBuffer, comp->color(i, 1), comp->extent().width, comp->extent().height);
            hw::loc::comp()->transitionImageLayout(comp->color(i, 1), VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);

            hw::loc::comp()->transitionImageLayout(comp->color(i, 2), VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
        }

        hw::loc::device()->destroy(stagingBuffer);
        hw::loc::device()->free(stagingBufferMemory);

        comp->addPipeline(desc->pipeLayout(2), "shaders/simulation.comp.spv");
    }

    void setupRender() {
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
                render->addPipeline(desc->pipeLayout(1), "shaders/quad.vert.spv", "shaders/quad.geom.spv", "shaders/quad.frag.spv", true, false);
            }
        }
    }

    void createSyncObjects()
    {
        computeFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
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
            hw::loc::device()->create(semaphoreInfo, computeFinishedSemaphores[i]);
            hw::loc::device()->create(semaphoreInfo, imageAvailableSemaphores[i]);
            hw::loc::device()->create(semaphoreInfo, renderFinishedSemaphores[i]);
            hw::loc::device()->create(fenceInfo, inFlightFences[i]);
        }
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
        delete comp;

    #ifdef IMGUI_ON
        imgui->cleanup();
    #endif
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
            hw::loc::device()->destroy(computeFinishedSemaphores[i]);
            hw::loc::device()->destroy(inFlightFences[i]);
        }

    #ifdef IMGUI_ON
        delete imgui;
    #endif
        delete camera;
        delete hw::loc::comp();
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

        setupCompute();
        setupRender();
        desc->allocate();

        createUniformBuffers();
        bindUnisToDescriptorSets();

    #ifdef IMGUI_ON
        imgui->adjust();
    #endif

        recordSimulationCommandBuffers();
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
                if (mesh->tag == "Simulation") {
                    create::buffer(sizeof(UserSimulationInput), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, desc->getUniBuffer(mesh, i, 0), desc->getUniMemory(mesh, i, 0));
                    continue;
                }

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

            VkDescriptorImageInfo imageInfo3 = {};
            imageInfo3.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

            std::array<VkWriteDescriptorSet, 4> descriptorWrites = {};
            descriptorWrites[0] = desc->writeSet(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0);
            descriptorWrites[0].pBufferInfo = &bufferInfo;

            descriptorWrites[1] = desc->writeSet(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1);
            descriptorWrites[1].pImageInfo = &imageInfo;

            descriptorWrites[2] = desc->writeSet(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0);
            descriptorWrites[2].pImageInfo = &imageInfo2;

            descriptorWrites[3] = desc->writeSet(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1);
            descriptorWrites[3].pImageInfo = &imageInfo3;

            VkDescriptorImageInfo computeImageInfo = {};
            computeImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

            VkDescriptorImageInfo computeImageInfo1 = {};
            computeImageInfo1.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

            VkDescriptorImageInfo computeImageInfo2 = {};
            computeImageInfo2.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

            VkDescriptorBufferInfo computeBuffer = {};
            computeBuffer.offset = 0;
            computeBuffer.range = sizeof(UserSimulationInput);

            std::array<VkWriteDescriptorSet, 4> computeWrites = {};
            computeWrites[0] = desc->writeSet(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 0);
            computeWrites[0].pImageInfo = &computeImageInfo;

            computeWrites[1] = desc->writeSet(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1);
            computeWrites[1].pImageInfo = &computeImageInfo1;

            computeWrites[2] = desc->writeSet(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2);
            computeWrites[2].pImageInfo = &computeImageInfo2;

            computeWrites[3] = desc->writeSet(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3);
            computeWrites[3].pBufferInfo = &computeBuffer;
            
            #pragma omp parallel for
            for (auto& mesh : desc->meshes) {
                if (mesh->tag == "Simulation") {
                    computeWrites[0].dstSet = desc->getDescriptor(mesh, i, 0);
                    computeWrites[1].dstSet = desc->getDescriptor(mesh, i, 0);
                    computeWrites[2].dstSet = desc->getDescriptor(mesh, i, 0);
                    computeWrites[3].dstSet = desc->getDescriptor(mesh, i, 0);

                    computeImageInfo.imageView = comp->colorView(0, 0);
                    computeImageInfo.sampler = comp->colorSampler(0, 0);

                    computeImageInfo1.imageView = comp->colorView(0, 1);
                    computeImageInfo1.sampler = comp->colorSampler(0, 1);

                    computeImageInfo2.imageView = comp->colorView(0, 2);
                    computeImageInfo2.sampler = comp->colorSampler(0, 2);

                    computeBuffer.buffer = desc->getUniBuffer(mesh, i, 0);

                    hw::loc::device()->update(static_cast<uint32_t>(4), computeWrites.data());
                    continue;
                }

                descriptorWrites[0].dstSet = desc->getDescriptor(mesh, i, 0);
                descriptorWrites[1].dstSet = desc->getDescriptor(mesh, i, 0);

                bufferInfo.buffer = desc->getUniBuffer(mesh, i, 0);

                if (mesh->tag == "Quad") {
                    descriptorWrites[2].dstSet = desc->getDescriptor(mesh, i, 1);
                    descriptorWrites[3].dstSet = desc->getDescriptor(mesh, i, 1);

                    imageInfo.imageView = refraction->colorView(i);
                    imageInfo.sampler = refraction->colorSampler(i);

                    imageInfo2.imageView = reflection->colorView(i);
                    imageInfo2.sampler = reflection->colorSampler(i);

                    imageInfo3.imageView = comp->colorView(0, 2);
                    imageInfo3.sampler = comp->colorSampler(0, 2);

                    hw::loc::device()->update(static_cast<uint32_t>(4), descriptorWrites.data());
                } else {
                    imageInfo.imageView = mesh->texture->view();
                    imageInfo.sampler = mesh->texture->sampler();
                    hw::loc::device()->update(static_cast<uint32_t>(2), descriptorWrites.data());
                }
            }
        }
    }

    void recordSimulationCommandBuffers() {
        for (uint32_t i = 0; i < hw::loc::swapChain()->size(); i++) {
            hw::loc::comp()->startBuffer(comp->commandBuffer(i));

            for (auto& mesh: desc->meshes) {
                if (mesh->tag == "Simulation") {

                    hw::loc::comp()->barrier(
                            comp->commandBuffer(i), 
                            0, VK_ACCESS_SHADER_READ_BIT, 
                            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
                        );

                    desc->bindDescriptors(comp->commandBuffer(i), mesh, i, 2, true);
                    vkCmdBindPipeline(comp->commandBuffer(i), VK_PIPELINE_BIND_POINT_COMPUTE, comp->pipeline(0));
                    vkCmdDispatch(comp->commandBuffer(i), comp->extent().width / 32, comp->extent().height / 32, 1);

                    hw::loc::comp()->imageBarrier(
                            comp->commandBuffer(i), comp->color(0, 1),
                            VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT, 
                            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
                        );
                    
                    hw::loc::comp()->imageBarrier(
                            comp->commandBuffer(i), comp->color(0, 0),
                            VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT, 
                            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
                        );

                    VkImageCopy imageCopy =  {
                                {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}, {0, 0, 0},
                                {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}, {0, 0, 0},
                                {comp->extent().width, comp->extent().height, 1}
                        };

                    vkCmdCopyImage(
                            comp->commandBuffer(i),
                            comp->color(0, 1), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                            comp->color(0, 0), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            1, &imageCopy
                        );

                    hw::loc::comp()->imageBarrier(
                            comp->commandBuffer(i), comp->color(0, 0),
                            VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, 
                            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL
                        );

                    hw::loc::comp()->imageBarrier(
                            comp->commandBuffer(i), comp->color(0, 1),
                            VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, 
                            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
                        );
                    
                    hw::loc::comp()->imageBarrier(
                            comp->commandBuffer(i), comp->color(0, 2),
                            VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, 
                            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
                        );

                    vkCmdCopyImage(
                            comp->commandBuffer(i),
                            comp->color(0, 2), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                            comp->color(0, 1), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            1, &imageCopy
                        );

                    hw::loc::comp()->imageBarrier(
                            comp->commandBuffer(i), comp->color(0, 1),
                            VK_ACCESS_SHADER_READ_BIT, 0, 
                            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL
                        );
                    
                    hw::loc::comp()->imageBarrier(
                            comp->commandBuffer(i), comp->color(0, 2),
                            VK_ACCESS_SHADER_READ_BIT, 0, 
                            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL
                        );
                }
            }

            hw::loc::comp()->endBuffer(comp->commandBuffer(i));
        }
    }

    void recordWaterCommandBuffers()
    {
        for (uint32_t i = 0; i < hw::loc::swapChain()->size(); i++) {
            hw::loc::cmd()->startBuffer(water->commandBuffer(i));
            water->startPass(i);

            VkDeviceSize offsets[] = { 0 };

            PushConstants pushConstants;

            #pragma omp parallel for
            for (auto& mesh : desc->meshes) {
                if (mesh->tag == "Simulation")
                    continue;

                vkCmdPushConstants(water->commandBuffer(i), desc->pipeLayout(0), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &pushConstants);

                if (mesh->tag != "Quad")
                    desc->bindDescriptors(water->commandBuffer(i), mesh, i, 0);
                else
                    desc->bindDescriptors(water->commandBuffer(i), mesh, i, 1);

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

            water->endPass(i);
            hw::loc::cmd()->endBuffer(water->commandBuffer(i));
        }
    }

    void recordRefractionCommandBuffers()
    {
        for (uint32_t i = 0; i < hw::loc::swapChain()->size(); i++) {
            hw::loc::cmd()->startBuffer(refraction->commandBuffer(i));
            refraction->startPass(i);

            VkDeviceSize offsets[] = { 0 };

            PushConstants pushConstants;
            pushConstants.clipPlane = glm::vec4(0.0f, -1.0f, 0.0f, 1.0f + 2.0f);

            #pragma omp parallel for
            for (auto& mesh : desc->meshes) {
                if (mesh->tag == "Simulation")
                    continue;

                if (mesh->tag == "Quad")
                    continue;

                vkCmdPushConstants(refraction->commandBuffer(i), desc->pipeLayout(0), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &pushConstants);

                desc->bindDescriptors(refraction->commandBuffer(i), mesh, i, 0);
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

            refraction->endPass(i);
            hw::loc::cmd()->endBuffer(refraction->commandBuffer(i));
        }
    }

    void recordReflectionCommandBuffers()
    {
        for (uint32_t i = 0; i < hw::loc::swapChain()->size(); i++) {
            hw::loc::cmd()->startBuffer(reflection->commandBuffer(i));
            reflection->startPass(i);

            VkDeviceSize offsets[] = { 0 };

            PushConstants pushConstants;
            pushConstants.clipPlane = glm::vec4(0.0f, 1.0f, 0.0f, -1.0f + 0.1f);
            pushConstants.invert = glm::vec3(1.0f);

            #pragma omp parallel for
            for (auto& mesh : desc->meshes) {
                if (mesh->tag == "Simulation")
                    continue;

                if (mesh->tag == "Quad")
                    continue;

                vkCmdPushConstants(reflection->commandBuffer(i), desc->pipeLayout(0), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &pushConstants);

                desc->bindDescriptors(reflection->commandBuffer(i), mesh, i, 0);
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

            reflection->endPass(i);
            hw::loc::cmd()->endBuffer(reflection->commandBuffer(i));
        }
    }

    void updateUniformBuffer(uint32_t currentImage)
    {
        UniformBufferObject ubo = {};

        ubo.proj = camera->proj;
        ubo.view = camera->view;
        ubo.invertView = camera->viewI;
        ubo.cameraPos = glm::vec4(camera->cameraPos, currentTime);

        #pragma omp parallel for
        for (auto& mesh : desc->meshes) {
            if (mesh->tag == "Simulation") {
                UserSimulationInput usi = {};
                usi.mouse = glm::vec4(0.0f);
                usi.mouse.z = (camera->mousePressed) ? 1.0f : 0.0f;

                void* data;
                hw::loc::device()->map(desc->getUniMemory(mesh, currentImage, 0), sizeof(usi), data);
                memcpy(data, &usi, sizeof(usi));
                hw::loc::device()->unmap(desc->getUniMemory(mesh, currentImage, 0));
                continue;
            }

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
    #ifdef IMGUI_ON
        // IMGUI
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        /* ImGui::Text("Clip Distance Change"); */
        /* ImGui::SliderFloat("Height", &clipPlane.w, -10.0f, 10.0f); */
        ImGui::Render();
    #endif

        hw::loc::device()->waitFence(inFlightFences[currentFrame]);

        uint32_t imageIndex;
        VkResult result = hw::loc::device()->nextImage(hw::loc::swapChain()->get(), imageAvailableSemaphores[currentFrame], imageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            recreateSwapChain();
        #ifdef IMGUI_ON
            ImGui_ImplVulkan_SetMinImageCount(hw::loc::swapChain()->size());
        #endif
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

    #ifdef IMGUI_ON
        // Render IMGUI
        imgui->recordCommandBuffer(imageIndex);
    #endif

        // Submit
        {
            std::vector<VkCommandBuffer> submitBuffers = {
                comp->commandBuffer(imageIndex),
            };

            VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[currentFrame] };
            VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT };
            VkSemaphore signalSemaphores[] = { computeFinishedSemaphores[currentFrame] };

            VkSubmitInfo submitInfo = {};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.waitSemaphoreCount = 1;
            submitInfo.pWaitSemaphores = waitSemaphores;
            submitInfo.pWaitDstStageMask = waitStages;
            submitInfo.commandBufferCount = static_cast<uint32_t>(submitBuffers.size());
            submitInfo.pCommandBuffers = submitBuffers.data();
            submitInfo.signalSemaphoreCount = 1;
            submitInfo.pSignalSemaphores = signalSemaphores;

            hw::loc::device()->submitCompute(submitInfo, VK_NULL_HANDLE);
        }

        {
            std::vector<VkCommandBuffer> submitCommandBuffers = {
                refraction->commandBuffer(imageIndex),
                reflection->commandBuffer(imageIndex),
                water->commandBuffer(imageIndex),
            #ifdef IMGUI_ON
                imgui->getCommandBuffer(imageIndex),
            #endif
            };

            VkSemaphore waitSemaphores[] = { computeFinishedSemaphores[currentFrame] };
            VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
            VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[currentFrame] };

            VkSubmitInfo submitInfo = {};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.waitSemaphoreCount = 1;
            submitInfo.pWaitSemaphores = waitSemaphores;
            submitInfo.pWaitDstStageMask = waitStages;
            submitInfo.commandBufferCount = static_cast<uint32_t>(submitCommandBuffers.size());
            submitInfo.pCommandBuffers = submitCommandBuffers.data();
            submitInfo.signalSemaphoreCount = 1;
            submitInfo.pSignalSemaphores = signalSemaphores;

            hw::loc::device()->reset(inFlightFences[currentFrame]);
            hw::loc::device()->submitGraphics(submitInfo, inFlightFences[currentFrame]);
        }

        // Present Frame
        {
            VkSwapchainKHR swapChains[] = { hw::loc::swapChain()->get() };

            VkSemaphore waitSemaphores[] = { renderFinishedSemaphores[currentFrame] };

            VkPresentInfoKHR presentInfo = {};
            presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            presentInfo.waitSemaphoreCount = 1;
            presentInfo.pWaitSemaphores = waitSemaphores;
            presentInfo.swapchainCount = 1;
            presentInfo.pSwapchains = swapChains;
            presentInfo.pImageIndices = &imageIndex;

            result = hw::loc::device()->present(presentInfo);

            if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
                framebufferResized = false;
                recreateSwapChain();
            #ifdef IMGUI_ON
                ImGui_ImplVulkan_SetMinImageCount(hw::loc::swapChain()->size());
            #endif
            } else if (result != VK_SUCCESS) {
                throw std::runtime_error("failed to present swap chain image!");
            }
        }

        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }
};
