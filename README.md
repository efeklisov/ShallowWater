# Shallow water simulation 💦

My attempts at Vulkan programming based on examples by [Overv](https://github.com/Overv/VulkanTutorial) and [Sascha Willems](https://github.com/SaschaWillems/Vulkan)

![Preview](preview.png)

# Installation 📲

## Compilation 🏭

```
mkdir build
cd build
cmake ..
make
```
## Shaders 🖌️

You need to install `parallel` from your repository and run 

```../recompile_shader.sh```

## Run 🏃‍♀️

To run the programm unzip `grid.obj` in `build/models` and execute

```./engine```

# Controls 🕹️

- **WASD+mouse** - 3D movement
- **Q/E** - roll the camera
- **Space/Backspace** - move up or down non-relative to the camera
- **Right mouse button** - send distortion to the water
- **R** - show water's vertex grid
- **Escape** - stop registering mouse movement
