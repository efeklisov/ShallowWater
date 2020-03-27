#include <vulkan/vulkan.h>

#include <glm/glm.hpp>

#include <vector>
#include <string>
#include <string_view>

#include "image.h"
#include "devloc.h"
#include "read.h"
#include "vertexloc.h"

struct Mesh {
    Mesh(std::string_view _tag, std::string_view model, std::unique_ptr<Image> _texture,
            glm::vec3 _transform=glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3 _rotation=glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3 _scale=glm::vec3(1.0f, 1.0f, 1.0f))
    : tag(_tag.data()), transform(_transform), rotation(_rotation), scale(_scale) {

        texture = std::move(_texture);
        read::model(model.data(), VertexLoc::vertices(), VertexLoc::indices(), vertex.start, vertex.size);
    }

    std::string tag;

    std::unique_ptr<Image> texture;
    VkPipeline* pipeline;

    glm::vec3 transform;
    glm::vec3 rotation;
    glm::vec3 scale;

    struct VertexBufferInfo {
        uint32_t start;
        uint32_t size;
    } vertex;

    struct DescriptorData {
        VkDescriptorSetLayout* layout;
        std::vector<VkDescriptorSet> sets;
    } descriptor;

    struct UniformData {
        std::vector<VkBuffer> buffers;
        std::vector<VkDeviceMemory> memory;
    } uniform;

    void free() {
        for (uint32_t i = 0; i < uniform.buffers.size(); i++) {
            DevLoc::device()->destroy(uniform.buffers[i]);
            DevLoc::device()->free(uniform.memory[i]);
        }
    }
};
