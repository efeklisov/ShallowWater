#pragma once

#include "device.h"

class DevLoc {
    public:
        static hw::Device* device() {
            assert(_device != NULL);
            return _device;
        }

        static void provide(hw::Device* service)
        {
            _device = service;
        }

    private:
        static hw::Device* _device;
};

hw::Device* DevLoc::_device;
