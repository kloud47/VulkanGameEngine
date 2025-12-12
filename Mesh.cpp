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
	VkDeviceSize bufferSize = sizeof(Vertex) * vertices->size();	// Calculate size of vertex buffer

	// Temporary buffer to "stage" vertex data before transfering it to GPU
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	// Create Staging Buffer and allocate Memory to it
	createBuffer(physicalDevice, device, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		stagingBuffer, stagingBufferMemory);

	// Map memory to vertex Buffer
	void* data;						// Create a pointer to point to the allocated memory
	vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);	// Map allocated memory to pointer
	memcpy(data, vertices->data(), (size_t)bufferSize);	// Copy vertex data to mapped memory
	vkUnmapMemory(device, stagingBufferMemory);	// Unmap the memory from the pointer

	// Create Buffer with TRANSFER_DST_BIT to move data from staging buffer to vertex buffer
	// Buffer memory is to be DEVICE_LOCAL_BIT meaning memory is on the GPU and only accessible by it and not the CPU (host)
	createBuffer(physicalDevice, device, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory);
}


