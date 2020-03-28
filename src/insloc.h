#pragma once

#include "instance.h"

class InsLoc {
    public:
        static hw::Instance* instance() {
            assert(_instance != NULL);
            return _instance;
        }

        static void provide(hw::Instance* service) {
            _instance = service;
        }

    private:
        static hw::Instance* _instance;
};

hw::Instance* InsLoc::_instance;
