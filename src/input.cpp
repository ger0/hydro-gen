#include "input.hpp"
#include "imgui.h"

using namespace Input_handling;

using glm::vec2, glm::vec3, glm::ivec2, glm::ivec3;
using glm::vec4, glm::ivec4;

void Input_handling::setup(GLFWwindow* window) {
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	glfwSetCursorPosCallback(window, mouse_callback);
	glfwSetKeyCallback(window, key_callback);
}

void Input_handling::mouse_callback(GLFWwindow *window, double xpos, double ypos) {
    auto& imo = ImGui::GetIO();

	float xoffset = xpos - mouse_last.x;
	// reversed since y-coordinates range from bottom to top
	float yoffset = mouse_last.y - ypos; 
	mouse_last.x = xpos;
	mouse_last.y = ypos;

    if (imo.WantCaptureMouse || State::global.mouse_disabled) {
        return;
    }

	const float sensitivity = 0.1f;
	xoffset *= sensitivity;
	yoffset *= sensitivity;

	camera.yaw += xoffset;
	camera.pitch += yoffset;
	if (camera.pitch > 89.f) {
		camera.pitch = 89.f;
	}
	if (camera.pitch < -89.f) {
		camera.pitch = -89.f;
	}
	vec3 direction;
	direction.x = cos(glm::radians(camera.yaw)) 
	    * cos(glm::radians(camera.pitch));
	direction.y = sin(glm::radians(camera.pitch));
	direction.z = sin(glm::radians(camera.yaw)) 
	    * cos(glm::radians(camera.pitch));
	camera.dir = normalize(direction);
}

void Input_handling::key_callback(GLFWwindow *window, 
        int key, int scancode, 
        int act, int mod) {

    auto& io = ImGui::GetIO();
    if (io.WantCaptureKeyboard) {
        return;
    }
    constexpr float speed_cap = 100.0f;
	float speed = 200.f * State::global.delta_frame * (camera.boost ? 8.f : 1.f);
	speed = speed > speed_cap ? speed_cap : speed;
    
	constexpr auto& gkey = glfwGetKey;
	if (gkey(window, GLFW_KEY_W) == GLFW_PRESS)
		camera.pos += speed * camera.dir;
	if (gkey(window, GLFW_KEY_S) == GLFW_PRESS)
		camera.pos -= speed * camera.dir;
	if (gkey(window, GLFW_KEY_A) == GLFW_PRESS)
		camera.pos -= 
			normalize(cross(camera.dir, camera.up)) * speed;
	if (gkey(window, GLFW_KEY_D) == GLFW_PRESS)
    	camera.pos += 
    		normalize(cross(camera.dir, camera.up)) * speed;
	if (gkey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
		camera.boost = true;
	}
	if (gkey(window, GLFW_KEY_SPACE) == GLFW_RELEASE) {
		camera.boost = false;
	}
	if (gkey(window, GLFW_KEY_LEFT_ALT)) {
	    auto curs = glfwGetInputMode(window, GLFW_CURSOR);
	    if (curs == GLFW_CURSOR_DISABLED) {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
        } else {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            io.ConfigFlags |= ImGuiConfigFlags_NoMouse;
        }
        State::global.mouse_disabled = !State::global.mouse_disabled;
	}
}
