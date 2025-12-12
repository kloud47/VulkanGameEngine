#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>	

#include <vector>

#include "utilities.h"

class Mesh
{
public:
	Mesh();
	Mesh(VkPhysicalDevice newPhysicalDevice, VkDevice newDevice, VkQueue transferQueue, 
		VkCommandPool transferCommandPool, std::vector<Vertex>* vertices);

	int getVertexCount();
	VkBuffer getVertexBuffer();

	void destroyVertexBuffer();

	~Mesh();

private:
	int vertexCount;
	VkBuffer vertexBuffer;
	VkDeviceMemory vertexBufferMemory;

	VkPhysicalDevice physicalDevice;
	VkDevice device;

	VkBuffer createVertexBuffer(VkQueue transferQueue, VkCommandPool transferCommandPool, std::vector<Vertex>* vertices);
};

