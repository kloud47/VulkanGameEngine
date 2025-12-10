#pragma once

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

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
		void draw();
		void cleanup();

	private:
		GLFWwindow* _window;

		int currentFrame = 0;

		// Scene Objects
		Mesh firstMesh;

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
		void createGraphicsPipeline();
		void createFramebuffers();
		void createCommandPool();
		void createCommandBuffers();
		void createSyncObjects();

		// - Record Functions
		void recordCommand();

		// - Get Functions
		void getPhysicalDevice();
		

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

