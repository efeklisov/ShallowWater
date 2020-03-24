#include <vulkan/vulkan.h>

#include <glm/glm.hpp>

#include <vector>

#include "image.h"
#include "devloc.h"

struct Mesh {
    Image* texture;

    glm::vec3 transform;
    glm::vec3 rotation;
    glm::vec3 scale;

    struct VertexBufferInfo {
        uint32_t start;
        uint32_t size;
    } vertex;

    struct DescriptorData {
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
