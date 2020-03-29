#pragma once

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#include <stdexcept>

#include "locator.h"
#include "instance.h"

namespace hw {
    class Surface {
        public:
            Surface(GLFWwindow* window) {
                if (glfwCreateWindowSurface(hw::loc::instance()->get(), window, nullptr, &surface) != VK_SUCCESS) {
                    throw std::runtime_error("failed to create window surface!");
                }
            }

            ~Surface() {
                vkDestroySurfaceKHR(hw::loc::instance()->get(), surface, nullptr);
            }

            VkSurfaceKHR& get() {
                return surface;
            }

        private:
            VkSurfaceKHR surface;
    };
}
