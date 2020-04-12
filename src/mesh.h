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
    Mesh(std::string_view _tag, uint32_t start, uint32_t size, glm::vec2 _dimensions,
            glm::vec3 _transform=glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3 _rotation=glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3 _scale=glm::vec3(1.0f, 1.0f, 1.0f))
        : tag(_tag.data()), transform(_transform), rotation(_rotation), scale(_scale) {

            descriptor.start = start;
            descriptor.size = size;
            read::quad(_dimensions, hw::loc::vertices(), hw::loc::indices(), vertex.start, vertex.size);
            simple = true;
        }

    Mesh(std::string_view _tag, uint32_t start, uint32_t size, std::string_view model, Image* _texture,
            glm::vec3 _transform=glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3 _rotation=glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3 _scale=glm::vec3(1.0f, 1.0f, 1.0f))
        : tag(_tag.data()), texture(_texture), transform(_transform), rotation(_rotation), scale(_scale) {

            descriptor.start = start;
            descriptor.size = size;
            read::model(model.data(), hw::loc::vertices(), hw::loc::indices(), vertex.start, vertex.size);
        }

    ~Mesh() {
        if (texture != nullptr)
            delete texture;
    }

    bool simple = false;
    std::string tag;

    Image* texture = nullptr;

    glm::vec3 transform;
    glm::vec3 rotation;
    glm::vec3 scale;

    struct VertexBufferInfo {
        uint32_t start;
        uint32_t size;
    } vertex;

    struct DescriptorData {
        uint32_t start;
        uint32_t size;
    } descriptor;

    struct UniformData {
        uint32_t start = -1;
        uint32_t size = 0;
    } uniform;
};
