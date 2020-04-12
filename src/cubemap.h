#pragma once

#include <string>

#include "locator.h"
#include "device.h"
#include "command.h"
#include "create.h"
#include "image.h"

class CubeMap : public Image {
    public:
        CubeMap(std::string filename) {
            createImage(filename);
            createImageView();
            createSampler();
        }

        ~CubeMap() {
            hw::loc::device()->destroy(textureSampler);
            hw::loc::device()->destroy(textureImageView);
            hw::loc::device()->destroy(textureImage);
            hw::loc::device()->free(textureImageMemory);
        }

        VkImageView& view() {
            return textureImageView;
        }

        VkSampler& sampler() {
            return textureSampler;
        }

    private:
        VkImage textureImage;
        VkDeviceMemory textureImageMemory;
        VkImageView textureImageView;
        VkSampler textureSampler;

        void createImage(std::string filename) {
            stbi_uc* pixels[6];
            int texWidth, texHeight, texChannels;
            pixels[0] = stbi_load((filename + "/posx.jpg").data(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
            pixels[1] = stbi_load((filename + "/negx.jpg").data(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
            pixels[2] = stbi_load((filename + "/posy.jpg").data(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
            pixels[3] = stbi_load((filename + "/negy.jpg").data(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
            pixels[4] = stbi_load((filename + "/posz.jpg").data(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
            pixels[5] = stbi_load((filename + "/negz.jpg").data(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
            VkDeviceSize imageSize = texWidth * texHeight * 4;
            VkDeviceSize cubeMapSize = texWidth * texHeight * 4 * 6;

            #pragma omp parallel for
            for (int i = 0; i < 6; i++)
                if (!pixels[i])
                    throw std::runtime_error("failed to load cubemap image!");

            VkBuffer stagingBuffer;
            VkDeviceMemory stagingBufferMemory;
            create::buffer(cubeMapSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

            void* data;
            hw::loc::device()->map(stagingBufferMemory, cubeMapSize, data);

            #pragma omp parallel for
            for (int i = 0; i < 6; i++)
                memcpy(static_cast<char *>(data) + (static_cast<size_t>(imageSize) * i), pixels[i], static_cast<size_t>(imageSize));
            hw::loc::device()->unmap(stagingBufferMemory);

            #pragma omp parallel for
            for (int i = 0; i < 6; i++)
                stbi_image_free(pixels[i]);

            create::cubemap(texWidth, texHeight, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textureImage, textureImageMemory);

            hw::loc::cmd()->transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 6);
            hw::loc::cmd()->copyBufferToImage(stagingBuffer, textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), 6);
            hw::loc::cmd()->transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 6);

            hw::loc::device()->destroy(stagingBuffer);
            hw::loc::device()->free(stagingBufferMemory);
        }

        void createImageView() {
            textureImageView = create::imageView(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, 6, VK_IMAGE_VIEW_TYPE_CUBE);
        }

        void createSampler() {
            create::sampler(textureSampler, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
        }
};
