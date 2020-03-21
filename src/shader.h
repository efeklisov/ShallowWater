#pragma once

#include <vulkan/vulkan.h>

#include <string_view>

#include "devloc.h"
#include "read.h"

class Shader {
    public:
        VkPipelineShaderStageCreateInfo info() {
            return shaderStageInfo;
        }

        Shader(std::string_view filename, VkShaderStageFlagBits stage) {
            auto code = read::file(filename);

            VkShaderModuleCreateInfo createInfo = {};
            createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            createInfo.codeSize = code.size();
            createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

            DevLoc::device()->create(createInfo, shaderModule);

            shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            shaderStageInfo.stage = stage;
            shaderStageInfo.module = shaderModule;
            shaderStageInfo.pName = "main";
        }

        ~Shader() {
            DevLoc::device()->destroy(shaderModule);
        }
    private:
        VkShaderModule shaderModule;
        VkPipelineShaderStageCreateInfo shaderStageInfo = {};
};
