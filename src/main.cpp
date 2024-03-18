#include <glm/glm.hpp>
#include "utils.hpp"

int main(int argc, char* argv[]) {
    uint seed;
	srand(time(NULL));
	if (argc == 2) {
		seed = strtoul(argv[1], NULL, 10);
		LOG_DBG("SEED: %u", seed);
	} else {
		seed = rand();
	}

	Uq_ptr<GLFWwindow, decltype(&destroy_window)> window(
		init_window(glm::uvec2{800, 600}, "Title"),
		destroy_window
	);
	assert(window.get() != nullptr);

    return 0;
}
