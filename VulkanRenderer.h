#pragma once

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#include <../glm/gtc/matrix_transform.hpp>

#include "Mesh.h"
#include <stdexcept>
#include <vector>
#include <array>
#include <set>
#include <iostream>
#include <string>
#include <algorithm>

#include "utilities.h"

namespace EngineCore {
	class VulkanRenderer
	{
	public:
		VulkanRenderer();
		~VulkanRenderer();

		int init(GLFWwindow* newWindow);

		void updateModel(int modelID, glm::mat4 newModel);

		void draw();
		void cleanup();

	private:
		GLFWwindow* _window;

		int currentFrame = 0;

		// Scene Objects
		std::vector<Mesh> meshList;

		// Scene Settings
		struct UboViewProjection {
			glm::mat4 projection;
			glm::mat4 view;
		} uboViewProjection;

		// Vulkan components
		VkInstance _instance;

		struct {
			VkPhysicalDevice physicalDevice;
			VkDevice logicalDevice;
		} mainDevice;

		VkQueue graphicsQueue;
		VkQueue presentationQueue;
		VkSurfaceKHR surface;
		VkSwapchainKHR swapchain;
		std::vector<SwapchainImage> swapChainImages;
		std::vector<VkFramebuffer> swapChainFramebuffers;
		std::vector<VkCommandBuffer> commandBuffers;

		// - Descriptors
		VkDescriptorSetLayout descriptorSetLayout;

		VkPushConstantRange pushConstantRange;

		VkDescriptorPool descriptorPool;
		std::vector<VkDescriptorSet> descriptorSets;

		std::vector<VkBuffer> vpUniformBuffers;
		std::vector<VkDeviceMemory> vpUniformBuffersMemory;

		std::vector<VkBuffer> modelDUniformBuffers;
		std::vector<VkDeviceMemory> modelDUniformBuffersMemory;


		//VkDeviceSize minUniformBufferOffset;
		//size_t modelUniformAlignment;
		//Model* modelTransferSpace;

		// - Pipeline
		VkPipeline graphicsPipeline;
		VkPipelineLayout pipelineLayout;
		VkRenderPass renderPass;

		// - Pools
		VkCommandPool graphicsCommandPool;

		// - Utility
		VkFormat swapChainImageFormat;
		VkExtent2D swapChainExtent;

		// - Sync objects
		std::vector<VkSemaphore> imageAvailableSemaphore;
		std::vector<VkSemaphore> renderFinishedSemaphore;

		std::vector<VkFence> drawFences;

		// (Vulkan methods) ------------------
		// 1) Create functions
		void createInstance();
		void createLogicalDevice();
		void createSurface();
		void createSwapChain();
		void createRenderPass();
		void createDescriptorSetlayout();
		void createPushConstantRange();
		void createGraphicsPipeline();
		void createFramebuffers();
		void createCommandPool();
		void createCommandBuffers();
		void createSyncObjects();
		
		void createUniformBuffers();
		void createDescriptorPool();
		void createDescriptorSets();

		void updateUniformBuffers(uint32_t imageIndex);

		// - Record Functions
		void recordCommand(uint32_t currentImage);

		// - Get Functions
		void getPhysicalDevice();

		// - Allocate Functions
		void allocateDynamicBufferTransferSpace();
		

		// 2) Support functions
		// - Checker Functions
		bool checkInstanceExtensionSupport(const std::vector<const char*>* checkExtensions);
		bool checkDeviceExtensionSupport(VkPhysicalDevice phyDevice);
		bool checkDeviceSuitable(VkPhysicalDevice device);

		// - Get Functions
		QueueFamilyIndices getQueueFamilies(VkPhysicalDevice device);
		SwapChainDetails getSwapChainDetails(VkPhysicalDevice device);

		// - Choose Functions
		VkSurfaceFormatKHR  chooseBestSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats);
		VkPresentModeKHR	chooseBestPresentationMode(const std::vector<VkPresentModeKHR>& presentationModes);
		VkExtent2D  chooseSwapExtent(const VkSurfaceCapabilitiesKHR &surfaceCapabilities);

		// - Create Functions
		VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
		VkShaderModule createShaderModule(const std::vector<char>& code);
	};
}

