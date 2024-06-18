# Procedural generation with hydraulic erosion
There are two modes of erosion available:


- Grid based erosion (shallow water model with virtual pipes)
- Particle based erosion (random raindrop simulation)


By default hydro-gen uses shallow water model for simulation. 


In order to change the model please uncomment PARTICLE_COUNT in glsl/bindings.glsl. This will enable particle based erosion and change the settings accordingly. 

## Dependencies

```
Cmake
C++20
OpenGL
fmt
GLEW
glfw3
```

## Building and running
Remember to clone with recursive-submodules option.
```
mkdir build
cd build
cmake ../
make
./hydro-gen
```

In order to change map size replace NOISE_SIZE in settings.hpp with a different value.
Dimensions of the window can be changed by changing WINDOW_W and WINDOW_H variables in main.cpp.
(These settings along with the ability to change erosion method will be changed later).
## Screenshots

### Grid-based erosion
<img src="https://github.com/ger0/external_repository/blob/main/pics/hydro-gen/1.png" width="720"/>
<img src="https://github.com/ger0/external_repository/blob/main/pics/hydro-gen/2.png" width="720"/>
<img src="https://github.com/ger0/external_repository/blob/main/pics/hydro-gen/3.png" width="720"/>

### Particle-based erosion
<img src="https://github.com/ger0/external_repository/blob/main/pics/hydro-gen/part1.png" width="720"/>
