#pragma once

#include "locator.h"
#include "device.h"
#include "command.h"
#include "create.h"
#include "image.h"

class Texture : public Image {
    public:
        Texture(std::string filename) {
            createImage(filename);
            createImageView();
            createSampler();
        }

        ~Texture() {
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
            int texWidth, texHeight, texChannels;
            stbi_uc* pixels = stbi_load(filename.data(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
            VkDeviceSize imageSize = texWidth * texHeight * 4;

            if (!pixels) {
                throw std::runtime_error("failed to load texture image!");
            }

            VkBuffer stagingBuffer;
            VkDeviceMemory stagingBufferMemory;
            create::buffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

            void* data;
            hw::loc::device()->map(stagingBufferMemory, imageSize, data);
            /**/memcpy(data, pixels, static_cast<size_t>(imageSize));
            hw::loc::device()->unmap(stagingBufferMemory);

            stbi_image_free(pixels);

            create::image(texWidth, texHeight, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, textureImage, textureImageMemory);

            hw::loc::cmd()->transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
            hw::loc::cmd()->copyBufferToImage(stagingBuffer, textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
            hw::loc::cmd()->transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

            hw::loc::device()->destroy(stagingBuffer);
            hw::loc::device()->free(stagingBufferMemory);
        }

        void createImageView() {
            textureImageView = create::imageView(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
        }

        void createSampler() {
            create::sampler(textureSampler);
        }
};
