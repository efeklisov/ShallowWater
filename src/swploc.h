#pragma once

#include "swapchain.h"

class SwpLoc {
    public:
        static hw::SwapChain* swapChain() {
            assert(_swapChain != NULL);
            return _swapChain;
        }

        static void provide(hw::SwapChain* service) {
            _swapChain = service;
        }

    private:
        static hw::SwapChain* _swapChain;
};

hw::SwapChain* SwpLoc::_swapChain;
