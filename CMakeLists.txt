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
find_package(unofficial-inih CONFIG REQUIRED)
find_package(OpenGL REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)

include_directories(${CMAKE_SOURCE_DIR})

file(GLOB SRC_FILES src/*.cpp)
file(GLOB GLSL_FILES "${CMAKE_SOURCE_DIR}/glsl/*.glsl")

set(GLSL_OUTPUT_DIR "${CMAKE_BINARY_DIR}/glsl")

foreach(GLSL_FILE ${GLSL_FILES}) 
    get_filename_component(GLSL_NAME ${GLSL_FILE} NAME)
    set(TGT "${GLSL_OUTPUT_DIR}/${GLSL_NAME}")
    add_custom_command(
        OUTPUT ${TGT}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${GLSL_FILE} ${TGT}
        DEPENDS ${GLSL_FILE}
        COMMENT "Copying shader files to output directory"
    )
    list(APPEND COPIED_SHADERS ${TGT})
endforeach()

get_target_property(IMGUI_INCLUDE_DIR imgui::imgui INTERFACE_INCLUDE_DIRECTORIES)

if(CMAKE_BUILD_TYPE MATCHES DEBUG)
    add_definitions(-DDEBUG)
endif()

add_custom_target(copy_shaders ALL DEPENDS ${COPIED_SHADERS})

add_executable(hydro-gen ${SRC_FILES})
add_dependencies(hydro-gen copy_shaders)
target_include_directories(hydro-gen PRIVATE ${IMGUI_INCLUDE_DIR}/backends)

target_link_libraries(hydro-gen PRIVATE fmt::fmt)
target_link_libraries(hydro-gen PRIVATE glfw)
target_link_libraries(hydro-gen PRIVATE GLEW::GLEW)
target_link_libraries(hydro-gen PRIVATE unofficial::inih::inireader)
target_link_libraries(hydro-gen PRIVATE OpenGL::GL)
target_link_libraries(hydro-gen PRIVATE imgui::imgui)
target_link_libraries(hydro-gen PRIVATE glm::glm)
