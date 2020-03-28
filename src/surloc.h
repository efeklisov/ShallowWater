#pragma once

#include "surface.h"

class SurLoc {
    public:
        static hw::Surface* surface() {
            assert(_surface != NULL);
            return _surface;
        }

        static void provide(hw::Surface* service) {
            _surface = service;
        }

    private:
        static hw::Surface* _surface;
};

hw::Surface* SurLoc::_surface;
