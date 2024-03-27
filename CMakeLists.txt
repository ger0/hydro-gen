cmake_minimum_required(VERSION 3.7)
project(hydro-gen)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
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

set(IMGUI_SRC_FILES
    submodules/imgui/imgui.cpp
    submodules/imgui/imgui_demo.cpp
    submodules/imgui/imgui_draw.cpp
    submodules/imgui/imgui_tables.cpp
    submodules/imgui/imgui_widgets.cpp
    submodules/imgui/backends/imgui_impl_glfw.cpp
    submodules/imgui/backends/imgui_impl_opengl3.cpp
)

if(CMAKE_BUILD_TYPE MATCHES DEBUG)
    add_definitions(-DDEBUG)
endif()

add_executable(hydro-gen ${SRC_FILES} ${IMGUI_SRC_FILES})
target_include_directories(hydro-gen PRIVATE submodules/imgui)
target_include_directories(hydro-gen PRIVATE submodules/imgui/backends)

target_link_libraries(hydro-gen fmt::fmt)
target_link_libraries(hydro-gen glfw)
target_link_libraries(hydro-gen GLEW::GLEW)
target_link_libraries(hydro-gen GL)
