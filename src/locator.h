#pragma once

#include <iostream>
#include <vector>
#include <assert.h>

class Vertex;

namespace hw {
    class Instance;
    class Surface;
    class Device;
    class SwapChain;
    class Command;

    class loc {
        public:
            static hw::Instance* instance() {
                assert(_instance != NULL);
                return _instance;
            }

            static hw::Surface* surface() {
                assert(_surface != NULL);
                return _surface;
            }

            static hw::Device* device() {
                assert(_device != NULL);
                return _device;
            }

            static hw::SwapChain* swapChain() {
                assert(_swapChain != NULL);
                return _swapChain;
            }

            static hw::Command* comp() {
                assert(_commandComp != NULL);
                return _commandComp;
            }

            static hw::Command* cmd() {
                assert(_command != NULL);
                return _command;
            }

            static std::vector<Vertex>& vertices() {
                return *_vertices;
            }

            static std::vector<uint32_t>& indices() {
                return *_indices;
            }

            static void provide(hw::Instance* service) {
                _instance = service;
            }

            static void provide(hw::Surface* service) {
                _surface = service;
            }

            static void provide(hw::Device* service) {
                _device = service;
            }

            static void provide(hw::SwapChain* service) {
                _swapChain = service;
            }

            static void provide(hw::Command* service, bool compute=false) {
                if (compute)
                    _commandComp = service;
                else _command = service;
            }

            static void provide(std::vector<Vertex>& service) {
                _vertices = &service;
            }

            static void provide(std::vector<uint32_t>& service) {
                _indices = &service;
            }

        private:
            static hw::Instance* _instance;
            static hw::Surface* _surface;
            static hw::Device* _device;
            static hw::SwapChain* _swapChain;
            static hw::Command* _command;
            static hw::Command* _commandComp;
            static std::vector<Vertex>* _vertices;
            static std::vector<uint32_t>* _indices;
    };
}

hw::Instance* hw::loc::_instance;
hw::Surface* hw::loc::_surface;
hw::Device* hw::loc::_device;
hw::SwapChain* hw::loc::_swapChain;
hw::Command* hw::loc::_command;
hw::Command* hw::loc::_commandComp;
std::vector<Vertex>* hw::loc::_vertices;
std::vector<uint32_t>* hw::loc::_indices;
