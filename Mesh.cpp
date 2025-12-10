#include "Mesh.h"

Mesh::Mesh()
{
}

Mesh::Mesh(VkPhysicalDevice newPhysicalDevice, VkDevice newDevice, std::vector<Vertex>* vertices)
{
	vertexCount = static_cast<uint32_t>(vertices->size());
	physicalDevice = newPhysicalDevice;
	device = newDevice;
	createVertexBuffer(vertices);
}

int Mesh::getVertexCount()
{
	return vertexCount;
}

VkBuffer Mesh::getVertexBuffer()
{
	return vertexBuffer;
}

void Mesh::destroyVertexBuffer()
{
	vkDestroyBuffer(device, vertexBuffer, nullptr);
	vkFreeMemory(device, vertexBufferMemory, nullptr);
}

Mesh::~Mesh()
{
}

VkBuffer Mesh::createVertexBuffer(std::vector<Vertex>* vertices)
{
	// Create Buffer
	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = sizeof(Vertex) * vertices->size();	// Size of buffer in bytes
	bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;	// Multiple types of buffer possible, use Vertex Buffer
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;		// Similar to command buffers, can be used across multiple queue families

	VkResult result = vkCreateBuffer(device, &bufferInfo, nullptr, &vertexBuffer);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("failed to create vertex buffer!");
	}

	// Get Buffer Memory Requirements
	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(device, vertexBuffer, &memRequirements);

	// Allocate Memory to Buffer
	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = 
	allocInfo.memoryTypeIndex = findMemoryTypeIndex(memRequirements.memoryTypeBits,		// Index of memory type on Physical Device that has required bit flags
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);	// VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT: CPU can access, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT: No need to flush after writes

	// Allocate memory to VkDeviceMemory
	result = vkAllocateMemory(device, &allocInfo, nullptr, &vertexBufferMemory);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate vertex buffer memory!");
	}

	// Allocate Memory to given vertex buffer
	vkBindBufferMemory(device, vertexBuffer, vertexBufferMemory, 0);

	// Map memory to vertex Buffer
	void* data;						// Create a pointer to point to the allocated memory
	vkMapMemory(device, vertexBufferMemory, 0, bufferInfo.size, 0, &data);	// Map allocated memory to pointer
	memcpy(data, vertices->data(), (size_t)bufferInfo.size);	// Copy vertex data to mapped memory
	vkUnmapMemory(device, vertexBufferMemory);	// Unmap the memory from the pointer
}


