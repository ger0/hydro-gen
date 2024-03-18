cmake_minimum_required(VERSION 3.7)
project(hydro-gen)
set(CMAKE_CXX_STANDARD 20)
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

find_package(fmt)
find_package(GLEW REQUIRED)
find_package(glfw3 REQUIRED)
find_package(OpenGL REQUIRED)

file(GLOB SRC_FILES src/*.cpp)

include_directories(${CMAKE_SOURCE_DIR})

add_executable(hydro-gen ${SRC_FILES})

# target_compile_features(hydro-gen PUBLIC cxx_std_17)
target_link_libraries(hydro-gen fmt::fmt)
target_link_libraries(hydro-gen glfw)
target_link_libraries(hydro-gen GLEW::GLEW)
target_link_libraries(hydro-gen GL)
