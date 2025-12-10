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

	// Main loop
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
		renderer.draw();
	}

	renderer.cleanup();

	// Cleanup and close the window
	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}