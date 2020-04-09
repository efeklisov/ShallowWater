#pragma once
#include <volk.h>

#include <glm/glm.hpp>

#include <vector>
#include <string>
#include <string_view>
#include <iostream>
#include <memory>

#include "image.h"
#include "locator.h"
#include "device.h"
#include "vertex.h"
#include "read.h"

struct Mesh {
    Mesh(std::string_view _tag, glm::vec2 _dimensions,
            glm::vec3 _transform=glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3 _rotation=glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3 _scale=glm::vec3(1.0f, 1.0f, 1.0f))
        : tag(_tag.data()), transform(_transform), rotation(_rotation), scale(_scale) {

            read::quad(_dimensions, hw::loc::vertices(), hw::loc::indices(), vertex.start, vertex.size);
            simple = true;
        }

    Mesh(std::string_view _tag, std::string_view model, std::unique_ptr<Image> _texture,
            glm::vec3 _transform=glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3 _rotation=glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3 _scale=glm::vec3(1.0f, 1.0f, 1.0f))
        : tag(_tag.data()), transform(_transform), rotation(_rotation), scale(_scale) {

            texture = std::move(_texture);
            read::model(model.data(), hw::loc::vertices(), hw::loc::indices(), vertex.start, vertex.size);
        }

    bool simple = false;
    std::string tag;

    std::unique_ptr<Image> texture;

    glm::vec3 transform;
    glm::vec3 rotation;
    glm::vec3 scale;

    struct VertexBufferInfo {
        uint32_t start;
        uint32_t size;
    } vertex;

    struct DescriptorData {
        std::vector<VkDescriptorSet> sets;
        std::vector<VkDescriptorSet> imgs;
        std::vector<VkDescriptorSet> im2s;
    } descriptor;

    struct UniformData {
        std::vector<VkBuffer> buffers;
        std::vector<VkDeviceMemory> memory;
    } uniform;

    void free() {
        for (uint32_t i = 0; i < uniform.buffers.size(); i++) {
            hw::loc::device()->destroy(uniform.buffers[i]);
            hw::loc::device()->free(uniform.memory[i]);
        }
    }
};
