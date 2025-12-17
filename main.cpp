#include <iostream>
#include <GLFW/glfw3.h>

#include "VulkanRenderer.h"

GLFWwindow* window;
EngineCore::VulkanRenderer renderer;

void initWindow(std::string wName = "Game Window", const int width = 800, const int height = 600)
{
	// Initialize GLFW
	glfwInit();

	// Set GLFW to not create an OpenGL context
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	window = glfwCreateWindow(width, height, wName.c_str(), nullptr, nullptr);
}

int main() {
	// Create Window
	initWindow("Vulkan Render Engine", 800, 600);

	// Initialize Vulkan Renderer with the created window
	if (renderer.init(window) == EXIT_FAILURE)
	{
		std::cerr << "Failed to initialize Vulkan Renderer" << std::endl;
		return EXIT_FAILURE;
	}

	float angle = 0.0f;
	float deltaTime = 0.0f; // Assuming a frame time of ~16ms for 60 FPS
	float lastTime = 0.0f;


	// Main loop
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		float now = glfwGetTime();
		deltaTime = now - lastTime;
		lastTime = now;

		angle += 10.0f * deltaTime;
		if (angle > 360.0f) { angle -= 360.0f; }

		glm::mat4 firstModel(1.0f);
		glm::mat4 secondModel(1.0f);

		firstModel = glm::translate(firstModel, glm::vec3(-2.0f, 0.0f, -5.0f));
		firstModel = glm::rotate(firstModel, glm::radians(angle), glm::vec3(0.0f, 0.0f, 1.0f));

		secondModel = glm::translate(secondModel, glm::vec3(2.0f, 0.0f, -5.0f));
		secondModel = glm::rotate(secondModel, glm::radians(-angle * 100), glm::vec3(0.0f, 0.0f, 1.0f));

		renderer.updateModel(0, firstModel);
		renderer.updateModel(1, secondModel);

		renderer.draw();
	}

	renderer.cleanup();

	// Cleanup and close the window
	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}