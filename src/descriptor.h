#pragma once

#include <volk.h>
#include <glm/glm.hpp>

#include <utility>
#include <vector>
#include <string_view>
#include <map>

#include "device.h"
#include "locator.h"
#include "swapchain.h"
#include "mesh.h"

struct StructDescriptorSetLayout {
    VkDescriptorSetLayout layout;
    std::vector<VkDescriptorType> types;
};

class Descriptor {
    public:
        VkDescriptorSetLayout& layout(uint32_t index) {
            return layoutTypes[index].layout;
        }

        VkPipelineLayout& pipeLayout(uint32_t index) {
            return pipeLayouts[index];
        }

        void addLayout(const std::vector<std::pair<VkDescriptorType, VkShaderStageFlagBits>> types)
        {
            std::vector<VkDescriptorSetLayoutBinding> bindings(types.size());
            StructDescriptorSetLayout mew;

            uint32_t index = 0;
            for (auto& binding : bindings) {
                mew.types.push_back(types[index].first);

                binding.binding = index;
                binding.descriptorCount = 1;
                binding.descriptorType = types[index].first;
                binding.pImmutableSamplers = nullptr;
                binding.stageFlags = types[index].second;

                index++;
            }

            VkDescriptorSetLayoutCreateInfo layoutInfo = {};
            layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            layoutInfo.bindingCount = bindings.size();
            layoutInfo.pBindings = bindings.data();

            hw::loc::device()->create(layoutInfo, mew.layout);

            layoutTypes.push_back(mew);
        }

        void addPipeLayout(const std::vector<uint32_t> layouts)
        {
            std::vector<VkDescriptorSetLayout> theChosen;
            theChosen.reserve(layouts.size());

            for (auto& layout: layouts)
                theChosen.push_back(layoutTypes[layout].layout);

            VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
            pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(theChosen.size());
            pipelineLayoutInfo.pSetLayouts = theChosen.data();

            VkPipelineLayout pipelineLayout;
            hw::loc::device()->create(pipelineLayoutInfo, pipelineLayout);

            pipeLayouts.push_back(pipelineLayout);
        }

        void addPipeLayout(const std::vector<uint32_t> layouts, const std::vector<VkPushConstantRange> ranges)
        {
            std::vector<VkDescriptorSetLayout> theChosen;
            theChosen.reserve(layouts.size());

            for (auto& layout: layouts)
                theChosen.push_back(layoutTypes[layout].layout);

            VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
            pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(theChosen.size());
            pipelineLayoutInfo.pSetLayouts = theChosen.data();

            pipelineLayoutInfo.pushConstantRangeCount = ranges.size();
            pipelineLayoutInfo.pPushConstantRanges = ranges.data();

            VkPipelineLayout pipelineLayout;
            hw::loc::device()->create(pipelineLayoutInfo, pipelineLayout);

            pipeLayouts.push_back(pipelineLayout);
        }

        void addMesh(std::string_view _tag, const std::vector<uint32_t> _sets, glm::vec2 _dimensions,
                glm::vec3 _transform=glm::vec3(0.0f, 0.0f, 0.0f),
                glm::vec3 _rotation=glm::vec3(0.0f, 0.0f, 0.0f),
                glm::vec3 _scale=glm::vec3(1.0f, 1.0f, 1.0f))
            {
                meshes.push_back(new Mesh(_tag, descriptorLayouts.size(), _sets.size(), _dimensions, _transform, _rotation, _scale));

                #pragma omp parallel for
                for (auto& set: _sets) {
                    for (auto& type: layoutTypes[set].types) {
                        descriptorTypes[type]++;

                        if (type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
                            if (meshes[meshes.size() - 1]->uniform.start == -1) {
                                meshes[meshes.size() - 1]->uniform.start = uniIndex;
                                meshes[meshes.size() - 1]->uniform.size = 1;
                            } else meshes[meshes.size() - 1]->uniform.size++; 

                            uniIndex++;
                        }
                    } descriptorLayouts.push_back(layoutTypes[set].layout);
                }
            }

        void addMesh(std::string_view _tag, const std::vector<uint32_t> _sets, std::string_view model, Image* _texture,
                glm::vec3 _transform=glm::vec3(0.0f, 0.0f, 0.0f),
                glm::vec3 _rotation=glm::vec3(0.0f, 0.0f, 0.0f),
                glm::vec3 _scale=glm::vec3(1.0f, 1.0f, 1.0f))
            {
                meshes.push_back(new Mesh(_tag, descriptorLayouts.size(), _sets.size(), model, _texture, _transform, _rotation, _scale));

                #pragma omp parallel for
                for (auto& set: _sets) {
                    for (auto& type: layoutTypes[set].types) {
                        descriptorTypes[type]++;

                        if (type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
                            if (meshes[meshes.size() - 1]->uniform.start == -1) {
                                meshes[meshes.size() - 1]->uniform.start = uniIndex;
                                meshes[meshes.size() - 1]->uniform.size = 1;
                            } else meshes[meshes.size() - 1]->uniform.size++; 

                            uniIndex++;
                        }
                    } descriptorLayouts.push_back(layoutTypes[set].layout);
                }
            }

        void allocate()
        {
            std::vector<VkDescriptorPoolSize> poolSizes;
            poolSizes.reserve(descriptorTypes.size());

            for (auto& [key, val]: descriptorTypes) {
                poolSizes.push_back({key, val * hw::loc::swapChain()->size()});
            }

            VkDescriptorPoolCreateInfo poolInfo = {};
            poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
            poolInfo.pPoolSizes = poolSizes.data();
            poolInfo.maxSets = hw::loc::swapChain()->size() * descriptorLayouts.size();

            hw::loc::device()->create(poolInfo, pool);

            descriptorSets.resize(hw::loc::swapChain()->size());

            #pragma omp parallel for
            for (auto& sets: descriptorSets) {
                VkDescriptorSetAllocateInfo allocInfo = {};
                allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
                allocInfo.descriptorPool = pool;
                allocInfo.descriptorSetCount = descriptorLayouts.size();
                allocInfo.pSetLayouts = descriptorLayouts.data();

                sets.resize(descriptorLayouts.size());
                hw::loc::device()->allocate(allocInfo, sets.data());
            }

            uniBuffers.resize(hw::loc::swapChain()->size());
            uniMemory.resize(hw::loc::swapChain()->size());

            for (uint32_t i = 0; i < hw::loc::swapChain()->size(); i++) {
                uniBuffers[i].resize(uniIndex);
                uniMemory[i].resize(uniIndex);
            }
        }

        void freePool() {
            hw::loc::device()->destroy(pool);

            #pragma omp parallel for
            for (uint32_t j = 0; j < uniBuffers.size(); j++)
                for (uint32_t i = 0; i < uniBuffers[j].size(); i++) {
                    hw::loc::device()->destroy(uniBuffers[j][i]);
                    hw::loc::device()->free(uniMemory[j][i]);
                }
        }

        Descriptor() {
            descriptorTypes[VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER] = 0;
            descriptorTypes[VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER] = 0;
        }

        ~Descriptor() {
            for (auto& layout: layoutTypes)
                hw::loc::device()->destroy(layout.layout);

            for (auto& layout: pipeLayouts)
                hw::loc::device()->destroy(layout);

            for (auto& mesh: meshes)
                delete mesh;
        }

        VkDescriptorSet& getDescriptor(Mesh* mesh, uint32_t frame, uint32_t descriptor) {
            if (mesh->descriptor.size <= descriptor)
                std::runtime_error("No more descriptors for mesh");
            return descriptorSets[frame][mesh->descriptor.start + descriptor];
        }

        void bindDescriptors(VkCommandBuffer& buffer, Mesh* mesh, uint32_t frame, uint32_t layout) {
            vkCmdBindDescriptorSets(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeLayout(layout), 0, mesh->descriptor.size, &descriptorSets[frame][mesh->descriptor.start], 0, nullptr);
        }

        VkBuffer& getUniBuffer(Mesh* mesh, uint32_t frame, uint32_t buffer) {
            if (mesh->uniform.size <= buffer)
                std::runtime_error("No more descriptors for mesh");

            return uniBuffers[frame][mesh->uniform.start + buffer];
        }

        VkDeviceMemory& getUniMemory(Mesh* mesh, uint32_t frame, uint32_t buffer) {
            if (mesh->uniform.size <= buffer)
                std::runtime_error("No more descriptors for mesh");

            return uniMemory[frame][mesh->uniform.start + buffer];
        }

        std::vector<Mesh*> meshes;

    private:
        std::vector<StructDescriptorSetLayout> layoutTypes;
        std::map<VkDescriptorType, uint32_t> descriptorTypes;
        VkDescriptorPool pool;

        uint32_t uniIndex = 0;
        std::vector<std::vector<VkBuffer>> uniBuffers;
        std::vector<std::vector<VkDeviceMemory>> uniMemory;

        std::vector<VkDescriptorSetLayout> descriptorLayouts;
        std::vector<std::vector<VkDescriptorSet>> descriptorSets;

        std::vector<VkPipelineLayout> pipeLayouts;
};
