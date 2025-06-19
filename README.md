# Procedural terrain generation with hydraulic erosion and thermal erosion on GPU

A procedural terrain generator with hydraulic erosion written in C++ and OpenGL compute shaders.

While running, press Left Alt to toggle between camera and cursor input.

There are two modes of hydraulic erosion available:

- Grid based erosion (shallow water model based on virtual pipes)
- Particle based erosion (raindrop simulation)

By default hydro-gen uses shallow water model for simulation. 

In order to change the model please uncomment PARTICLE_COUNT in glsl/bindings.glsl. This will enable particle based erosion and change the settings accordingly. 

Erosion parameters include:
- Total transport capacity of a particle / unit of water
- Speed of dissolution
- Speed of deposition
- Water evaporation speed
- Talus angle of the material
- Slippage erosion speed

After changing the in-game parameters, press the "Set Erosion Settings" button to sent the updated parameters to erosion shaders.

## Dependencies
In order to run the program a GPU with the OpenGL 4.6 support is required.

### Building dependencies
#### GNU/Linux:
* C++23
* CMake
* Ninja
* vcpkg with the `$VCPKG_ROOT` environment variable set 
#### Windows:
Same as above or:
* Microsoft Visual Studio with MSVC, Ninja and CMake modules

If you don't want to use vcpkg then following libraries are required:

```
OpenGL
fmt
GLEW
glm
glfw3
Imgui
```

The building scripts use Ninja by default, you can change this manually in CMakePresets.json
## Building and running

```
git clone https://github.com/ger0/hydro-gen
cd hydro-gen
cmake --preset debug
cd build
ninja all
./hydro-gen
```

In order to change map size replace NOISE_SIZE in settings.hpp with a different value.


Dimensions of the window can be changed by changing WINDOW_W and WINDOW_H variables in main.cpp.


(These settings along with the ability to change erosion method will be changed later).
## Screenshots

<img src="https://github.com/ger0/external_repository/blob/main/pics/hydro-gen/new.webp" width="720"/>
<img src="https://github.com/ger0/external_repository/blob/main/pics/hydro-gen/new1.webp" width="720"/>
<img src="https://github.com/ger0/external_repository/blob/main/pics/hydro-gen/old.webp" width="720"/>
<img src="https://github.com/ger0/external_repository/blob/main/pics/hydro-gen/old1.webp" width="720"/>
<img src="https://github.com/ger0/external_repository/blob/main/pics/hydro-gen/old2.webp" width="720"/>
<img src="https://github.com/ger0/external_repository/blob/main/pics/hydro-gen/terracing.webp" width="720"/>
<img src="https://github.com/ger0/external_repository/blob/main/pics/hydro-gen/terracing1.webp" width="720"/>
