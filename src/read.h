#pragma once

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <glm/gtx/string_cast.hpp>

#include <vector>
#include <unordered_map>
#include <string_view>
#include <iostream>

#include "vertex.h"

namespace read {
    void model(std::string_view filename, std::vector<Vertex>& vertices, 
            std::vector<uint32_t>& indices) {
        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string warn, err;

        if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filename.data())) {
            throw std::runtime_error(warn + err);
        }

        std::unordered_map<Vertex, uint32_t> uniqueVertices = {};

        for (const auto& shape : shapes) {
            for (const auto& index : shape.mesh.indices) {
                Vertex vertex = {};

                vertex.pos = {
                    attrib.vertices[3 * index.vertex_index + 0],
                    attrib.vertices[3 * index.vertex_index + 1],
                    attrib.vertices[3 * index.vertex_index + 2]
                };

                vertex.texCoord = {
                    attrib.texcoords[2 * index.texcoord_index + 0],
                    1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
                };

                vertex.color = {1.0f, 1.0f, 1.0f};

                if (uniqueVertices.count(vertex) == 0) {
                    uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
                    vertices.push_back(vertex);
                }

                indices.push_back(uniqueVertices[vertex]);
            }
        }
    }

    void model(std::string_view filename, std::vector<Vertex>& vertices,
            std::vector<uint32_t>& indices, uint32_t& start, uint32_t& size) {

        std::vector<Vertex> _vertices;
        std::vector<uint32_t> _indices;

        model(filename, _vertices, _indices);

        size = _indices.size();
        start = vertices.size();

        vertices.insert(vertices.end(), _vertices.begin(), _vertices.end());
        indices.insert(indices.end(), _indices.begin(), _indices.end());
    }

    std::vector<char> file(std::string_view filename) {
        std::ifstream _file(filename.data(), std::ios::ate | std::ios::binary);

        if (!_file.is_open()) {
            throw std::runtime_error("failed to open file!");
        }

        size_t fileSize = (size_t) _file.tellg();
        std::vector<char> buffer(fileSize);

        _file.seekg(0);
        _file.read(buffer.data(), fileSize);

        _file.close();

        return buffer;
    }
}
