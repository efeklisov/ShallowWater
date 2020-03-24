#pragma once

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <vulkan/vulkan.h>

#include <string>

class Image {
    public:
        virtual VkImageView& view() = 0;
        virtual VkSampler& sampler() = 0;
        virtual ~Image() = 0;

    protected:
        virtual void createImage(std::string filename) = 0;
        virtual void createImageView() = 0;
        virtual void createSampler() = 0;
};

Image::~Image() {}
