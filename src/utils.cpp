#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "utils.hpp"

void GLAPIENTRY gl_error_callback(
    GLenum source,
    GLenum type,
    GLuint id,
    GLenum severity,
    GLsizei length,
    const GLchar* message,
    const void* user
) {
    (void)id;
    (void)length;
    (void)user;

    if(severity == GL_DEBUG_SEVERITY_NOTIFICATION) {
        return;
    }

    const char* type_str;

    switch(type) {
        case GL_DEBUG_TYPE_ERROR: type_str = "ERROR"; break;
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: type_str = "DEPRECATED"; break;
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR: type_str = "UNDEFINED"; break;
        case GL_DEBUG_TYPE_PORTABILITY: type_str = "UNPORTABLE"; break;
        case GL_DEBUG_TYPE_PERFORMANCE: type_str = "PERFORMANCE"; break;
        case GL_DEBUG_TYPE_MARKER: type_str = "MARKER"; break;
        case GL_DEBUG_TYPE_PUSH_GROUP: type_str = "PUSH"; break;
        case GL_DEBUG_TYPE_POP_GROUP: type_str = "POP"; break;
        default:
        case GL_DEBUG_TYPE_OTHER: type_str = "OTHER"; break;
    }

    const char* source_str;
    switch(source) {
        case GL_DEBUG_SOURCE_API: source_str = "API"; break;
        case GL_DEBUG_SOURCE_WINDOW_SYSTEM: source_str = "WINDOW"; break;
        case GL_DEBUG_SOURCE_SHADER_COMPILER: source_str = "SHADER"; break;
        case GL_DEBUG_SOURCE_THIRD_PARTY: source_str = "THIRD_PARTY"; break;
        case GL_DEBUG_SOURCE_APPLICATION: source_str = "USER"; break;
        default:
        case GL_DEBUG_SOURCE_OTHER: source_str = "OTHER"; break;
    }

    fprintf(
        stderr,
        "GL_%s, source = %s, message = %s\n",
        type_str,
        source_str,
        message
    );
    if(type == GL_DEBUG_TYPE_ERROR) {
        abort();
    }
}

GLFWwindow* init_window(glm::uvec2 window_size, const char* window_title) {
    glfwInit();

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    GLFWwindow* win = glfwCreateWindow(window_size.x, window_size.y, window_title, NULL, NULL);
    if (win == nullptr) {
        LOG_ERR("Failed to create window.");
        return nullptr;
    }
    glfwMakeContextCurrent(win);
    glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSwapInterval(1);

    if (glewInit() != GLEW_OK) {
        LOG_ERR("Failed to initialise GLEW!");
        return nullptr;
    }
    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(gl_error_callback, 0);
	
	/* IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	ImGui::StyleColorsClassic();
	ImGui_ImplGlfw_InitForOpenGL(window.get(), true);
	ImGui_ImplOpenGL3_Init(glsl_version); */

    return win;
}

void destroy_window(GLFWwindow* win) {
	glfwDestroyWindow(win);
	glfwTerminate();
};
