#pragma once

#include <vector>

#include "vertex.h"

class VertexLoc {
    public:
        static std::vector<Vertex>& vertices() {
            return *_vertices;
        }

        static std::vector<uint32_t>& indices() {
            return *_indices;
        }

        static void provide(std::vector<Vertex>& service) {
            _vertices = &service;
        }

        static void provide(std::vector<uint32_t>& service) {
            _indices = &service;
        }

    private:
        static std::vector<Vertex>* _vertices;
        static std::vector<uint32_t>* _indices;
};

std::vector<Vertex>* VertexLoc::_vertices;
std::vector<uint32_t>* VertexLoc::_indices;
