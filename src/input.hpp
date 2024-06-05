#ifndef HYDR_INPUT_HPP
#define HYDR_INPUT_HPP

#include "state.hpp"
#include <glm/glm.hpp>

namespace Input_handling {

// mouse position
static glm::vec2 mouse_last;

void setup(GLFWwindow* window);

static struct Camera {
    glm::vec3 pos    = glm::vec3(0.f, MAX_HEIGHT, 0.f);
    glm::vec3 dir    = glm::vec3(0.f, MAX_HEIGHT, -1.f);

    glm::vec3 _def_dir = normalize(pos - dir);
	static constexpr float speed = 0.05f;

    glm::vec3 right  = glm::normalize(glm::cross(glm::vec3(0,1,0), _def_dir));
	glm::vec3 up     = glm::cross(_def_dir, right);

	float yaw   = -90.f;
	float pitch = 0.f;

	bool boost  = false;	
} camera;

void mouse_callback(GLFWwindow *window, double xpos, double ypos);
void key_callback(GLFWwindow *window, 
        int key, int scancode, 
        int act, int mod);
};

#endif //HYDR_INPUT_HPP
