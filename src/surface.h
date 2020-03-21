#pragma once

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#include <stdexcept>

namespace hw {
    class Surface {
        public:
            Surface(VkInstance* _instance, GLFWwindow* window) : instance(_instance) {
                if (glfwCreateWindowSurface(*instance, window, nullptr, &surface) != VK_SUCCESS) {
                    throw std::runtime_error("failed to create window surface!");
                }
            }

            ~Surface() {
                vkDestroySurfaceKHR(*instance, surface, nullptr);
            }

            VkSurfaceKHR& get() {
                return surface;
            }

        private:
            VkInstance* instance;
            VkSurfaceKHR surface;
    };
}
