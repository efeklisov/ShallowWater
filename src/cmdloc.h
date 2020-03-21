#pragma once

#include <set>

#include "command.h"

class CmdLoc {
    public:
        static hw::Command* cmd() {
            assert(_cmd != NULL);
            return _cmd;
        }

        static void provide(hw::Command* service)
        {
            _cmd = service;
        }

    private:
        static hw::Command* _cmd;
};

hw::Command* CmdLoc::_cmd;
