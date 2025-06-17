cmake_minimum_required(VERSION 3.7)
project(hydro-gen)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

find_package(fmt CONFIG REQUIRED)
find_package(GLEW REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(OpenGL REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)

file(GLOB SRC_FILES src/*.cpp)
include_directories(${CMAKE_SOURCE_DIR})

get_target_property(IMGUI_INCLUDE_DIR imgui::imgui INTERFACE_INCLUDE_DIRECTORIES)

if(CMAKE_BUILD_TYPE MATCHES DEBUG)
    add_definitions(-DDEBUG)
endif()

add_executable(hydro-gen ${SRC_FILES})
target_include_directories(hydro-gen PRIVATE ${IMGUI_INCLUDE_DIR}/backends)

target_link_libraries(hydro-gen PRIVATE fmt::fmt)
target_link_libraries(hydro-gen PRIVATE glfw)
target_link_libraries(hydro-gen PRIVATE GLEW::GLEW)
target_link_libraries(hydro-gen PRIVATE OpenGL::GL)
target_link_libraries(hydro-gen PRIVATE imgui::imgui)
target_link_libraries(hydro-gen PRIVATE glm::glm)

add_custom_command(TARGET hydro-gen POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/glsl
            $<TARGET_FILE_DIR:hydro-gen>/glsl
    COMMENT "Copying GLSL files to output directory"
)
