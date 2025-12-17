#include "VulkanRenderer.h"

namespace EngineCore {
	VulkanRenderer::VulkanRenderer()
	{
		std::cout << "Vulkan Renderer created." << std::endl;
	}

	VulkanRenderer::~VulkanRenderer()
	{
	}


	int VulkanRenderer::init(GLFWwindow* newWindow)
	{
		_window = newWindow;

		// Initialization code for Vulkan would go here
		try {
			createInstance();
			createSurface();
			getPhysicalDevice();
			createLogicalDevice();
			createSwapChain();
			createRenderPass();
			createDescriptorSetlayout();
			createPushConstantRange();
			createGraphicsPipeline();
			createFramebuffers();
			createCommandPool();

			// UboViewProjection matrix setup
			uboViewProjection.projection = glm::perspective(glm::radians(45.0f), (float)swapChainExtent.width / (float)swapChainExtent.height, 0.1f, 100.0f);
			uboViewProjection.view = glm::lookAt(glm::vec3(0.0f, 0.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
			//uboViewProjection.model = glm::mat4(1.0f);

			uboViewProjection.projection[1][1] *= -1; // Invert Y axis for Vulkan	

			// Create a mesh
			
			// vertex Data
			std::vector<Vertex> meshVertices = {
				{ { -0.4, 0.4, 0.0 },{ 1.0f, 0.0f, 0.0f } },	// 0
			{ { -0.4, -0.4, 0.0 },{ 0.0f, 1.0f, 0.0f } },	    // 1
			{ { 0.4, -0.4, 0.0 },{ 0.0f, 0.0f, 1.0f } },    // 2
			{ { 0.4, 0.4, 0.0 },{ 1.0f, 1.0f, 0.0f } },   // 3

			};

			std::vector<Vertex> meshVertices2 = {
				{ { -0.25, 0.6, 0.0 },{ 1.0f, 0.0f, 0.0f } },	// 0
			{ { -0.25, -0.6, 0.0 },{ 0.0f, 1.0f, 0.0f } },	    // 1
			{ { 0.25, -0.6, 0.0 },{ 0.0f, 0.0f, 1.0f } },    // 2
			{ { 0.25, 0.6, 0.0 },{ 1.0f, 1.0f, 0.0f } },   // 3

			};

			// Index data
			std::vector<uint32_t> meshIndices = {
				0, 1, 2, // First Triangle
				2, 3, 0  // Second Triangle
			};

			Mesh firstMesh = Mesh(mainDevice.physicalDevice, mainDevice.logicalDevice, graphicsQueue, graphicsCommandPool, &meshVertices, &meshIndices);
			Mesh secondMesh = Mesh(mainDevice.physicalDevice, mainDevice.logicalDevice, graphicsQueue, graphicsCommandPool, &meshVertices2, &meshIndices);

			meshList.push_back(firstMesh);
			meshList.push_back(secondMesh);

			createCommandBuffers();
			//allocateDynamicBufferTransferSpace();
			createUniformBuffers();
			createDescriptorPool();
			createDescriptorSets();
			createSyncObjects();
		} 
		catch (const std::runtime_error& e) {
			std::string errorMessage = "Failed to create Vulkan instance: " + std::string(e.what());
			throw std::runtime_error(errorMessage);
			return EXIT_FAILURE;
		}

		return 0;
	}

	void VulkanRenderer::updateModel(int modelID,  glm::mat4 newModel)
	{
		if (modelID >= meshList.size()) return;

		meshList[modelID].setModel(newModel);
	}

	void VulkanRenderer::draw()
	{
		// 1. Get image from swap chain to draw to
		// 2. Execute command buffer with that image as attachment in framebuffer
		// 3. Return the image to the swap chain for presentation


		vkWaitForFences(mainDevice.logicalDevice, 1, &drawFences[currentFrame], VK_TRUE, std::numeric_limits<uint64_t>::max()); // Wait until the fence is signaled // CPU-GPU sync
		vkResetFences(mainDevice.logicalDevice, 1, &drawFences[currentFrame]); // Reset the fence to unsignaled state for next frame

		// - Get image from swap chain --------------------------------------------------------------------------
		uint32_t imageIndex; // Index of swap chain image to draw to and signal the semaphore when ready
		vkAcquireNextImageKHR(mainDevice.logicalDevice, swapchain, std::numeric_limits<uint64_t>::max(), imageAvailableSemaphore[currentFrame], VK_NULL_HANDLE, &imageIndex);

		// - Update uniform buffer ------------------------------------------------------------------------------
		recordCommand(imageIndex); // Record command buffer for this image
		updateUniformBuffers(imageIndex);

		// - Execute command buffer -----------------------------------------------------------------------------
		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.waitSemaphoreCount = 1;										// Number of semaphores to wait on before execution begins
		submitInfo.pWaitSemaphores = &imageAvailableSemaphore[currentFrame];					// Semaphores to wait on before execution begins
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT }; // Stages to check for before execution begins
		submitInfo.pWaitDstStageMask = waitStages;								// Stages to check for before execution begins
		submitInfo.commandBufferCount = 1;										// Number of command buffers to submit for execution
		submitInfo.pCommandBuffers = &commandBuffers[imageIndex];				// Command buffers to submit for execution
		submitInfo.signalSemaphoreCount = 1;										// Number of semaphores to signal once command buffer finishes execution
		submitInfo.pSignalSemaphores = &renderFinishedSemaphore[currentFrame];					// Semaphores to signal once command buffer finishes execution
		
		if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, drawFences[currentFrame]) != VK_SUCCESS) // Fence to signal when command buffer finishes execution
		{
			throw std::runtime_error("Failed to submit draw command buffer!");
		}

		// - Return the image to the swap chain for presentation -------------------------------------------------
		VkPresentInfoKHR presentInfo = {};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;										// Number of semaphores to wait on before presentation can happen
		presentInfo.pWaitSemaphores = &renderFinishedSemaphore[currentFrame];					// Semaphores to wait on before presentation can happen
		presentInfo.swapchainCount = 1;											// Number of swap chains to present images to
		presentInfo.pSwapchains = &swapchain;									// Swap chains to present images to
		presentInfo.pImageIndices = &imageIndex;								// Indices of images in swap chains to present
		presentInfo.pResults = nullptr;											// Optional return results for each swap chain
		
		// Present the image in the swap chain
		VkResult result = vkQueuePresentKHR(presentationQueue, &presentInfo);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to present swap chain image!");
		}

		currentFrame = (currentFrame + 1) % MAX_FRAME_DRAWS;
	}

	void VulkanRenderer::cleanup()
	{
		vkDeviceWaitIdle(mainDevice.logicalDevice); // Wait until no action is being run on device before destroying

		//_aligned_free(modelTransferSpace); // Free model transfer space C style function

		vkDestroyDescriptorPool(mainDevice.logicalDevice, descriptorPool, nullptr);
		vkDestroyDescriptorSetLayout(mainDevice.logicalDevice, descriptorSetLayout, nullptr);

		for (size_t i = 0; i < swapChainImages.size(); i++)
		{
			vkDestroyBuffer(mainDevice.logicalDevice, vpUniformBuffers[i], nullptr);
			vkFreeMemory(mainDevice.logicalDevice, vpUniformBuffersMemory[i], nullptr);
			/*vkDestroyBuffer(mainDevice.logicalDevice, modelDUniformBuffers[i], nullptr);
			vkFreeMemory(mainDevice.logicalDevice, modelDUniformBuffersMemory[i], nullptr);*/
		}

		for (size_t i = 0; i < meshList.size(); i++)
		{
			meshList[i].destroyBuffers();
		}

		for (size_t i = 0; i < MAX_FRAME_DRAWS; i++)
		{
			vkDestroySemaphore(mainDevice.logicalDevice, renderFinishedSemaphore[i], nullptr);
			vkDestroySemaphore(mainDevice.logicalDevice, imageAvailableSemaphore[i], nullptr);
			vkDestroyFence(mainDevice.logicalDevice, drawFences[i], nullptr);
		}
		vkDestroyCommandPool(mainDevice.logicalDevice, graphicsCommandPool, nullptr);
		for (auto frameBuffer : swapChainFramebuffers)
		{
			vkDestroyFramebuffer(mainDevice.logicalDevice, frameBuffer, nullptr);
		}
		vkDestroyPipeline(mainDevice.logicalDevice, graphicsPipeline, nullptr); 
		vkDestroyPipelineLayout(mainDevice.logicalDevice, pipelineLayout, nullptr);
		vkDestroyRenderPass(mainDevice.logicalDevice, renderPass, nullptr);
		for (auto image : swapChainImages)
		{
			vkDestroyImageView(mainDevice.logicalDevice, image.imageView, nullptr);
		}
		vkDestroySwapchainKHR(mainDevice.logicalDevice, swapchain, nullptr);
		vkDestroySurfaceKHR(_instance, surface, nullptr);
		vkDestroyDevice(mainDevice.logicalDevice, nullptr);
		vkDestroyInstance(_instance, nullptr);
		std::cout << "Vulkan Renderer destroyed." << std::endl;
	}
	void VulkanRenderer::createInstance()
	{
		// Application information
		// Most of the information here is optional and is only for developer convenience
		VkApplicationInfo appInfo{};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "Vulkan App";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "No Engine";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_0;

		// Creation information Vulkan instance
		VkInstanceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		//createInfo.pNext = nullptr;
		//createInfo.flags = 0;
		createInfo.pApplicationInfo = &appInfo; // Optional

		// Create list to hold instance extensions
		std::vector<const char*> extensions = std::vector<const char*>();

		uint32_t glfwExtensionCount = 0; // Holds number of extensions required by GLFW
		const char** glfwExtensions; // Holds array of extensions required by GLFW

		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount); // Get extensions required by GLFW

		// Add GLFW extensions to list of extensions
		for (uint32_t i = 0; i < glfwExtensionCount; i++) {
			extensions.push_back(glfwExtensions[i]);
		}

		if (!checkInstanceExtensionSupport(&extensions)) {
			throw std::runtime_error("Vulkan instance extensions required by GLFW are not supported!");
		}

		createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size()); // Optional
		createInfo.ppEnabledExtensionNames = extensions.data(); // Optional

		// No validation layers for now // application runs much smoother:
		createInfo.enabledLayerCount = 0; // Optional
		createInfo.ppEnabledLayerNames = nullptr; // Optional

		// Create the Vulkan instance --------------------------------------------------------------------------------------->
		VkResult result = vkCreateInstance(&createInfo, nullptr, &_instance);
		if (result != VK_SUCCESS) {
			throw std::runtime_error("failed to create instance!");
		}
	}

	void VulkanRenderer::createLogicalDevice()
	{
		//  Get the queue family indices for the chosen Physical device
		QueueFamilyIndices indices = getQueueFamilies(mainDevice.physicalDevice);

		// vector for queue creation information and set for family indices
		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		std::set<int> queueFamilyIndices = { indices.graphicsFamily, indices.presentationFamily };

		// Queue that the logical device needs to create and the info to do so (Only one for now will add more later!)
		for (int queueFamilyIndex : queueFamilyIndices) 
		{
			VkDeviceQueueCreateInfo queueCreateInfo{};
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = queueFamilyIndex;  // Index of the queue family to create the queue from
			queueCreateInfo.queueCount = 1;								// Number of queues to create from this family
			float queuePriority = 1.0f;									// Priority of the queue to create (0.0 - 1.0)
			queueCreateInfo.pQueuePriorities = &queuePriority;			// Pointer to the priority (or array of if creating multiple queues)
			
			queueCreateInfos.push_back(queueCreateInfo);
		}

		// Information to create the logical device (sometimes called "device")
		VkDeviceCreateInfo deviceCreateInfo{};
		deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());						// Number of entries in the queue create info array
		deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();			// Pointer to queue create info array
		deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(queueCreateInfos.size());						// number of enabled logical device extensions
		deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data()	;				// Pointer to array of enabled logical device extensions
		
		// Physical device features to be used by the logical device
		VkPhysicalDeviceFeatures deviceFeatures{};
		deviceCreateInfo.pEnabledFeatures = &deviceFeatures;			// Physical device features logical device will use

		// Create the logical device ---------------------------------------------------------------------------------------->
		VkResult result = vkCreateDevice(mainDevice.physicalDevice, &deviceCreateInfo, nullptr, &mainDevice.logicalDevice);
		if (result != VK_SUCCESS) {
			throw std::runtime_error("failed to create logical device!");
		}

		// Queues are created at the same time as the device....
		// So we want handle to queue
		// From given logical device, of given Queue Family, of given Queue Index (0 since only one queue)
		vkGetDeviceQueue(mainDevice.logicalDevice, indices.graphicsFamily, 0, &graphicsQueue);
		vkGetDeviceQueue(mainDevice.logicalDevice, indices.presentationFamily, 0, &presentationQueue);
	}

    void VulkanRenderer::createSurface()
    {
        // Create window surface using GLFW helper function
        if (glfwCreateWindowSurface(_instance, _window, nullptr, &surface) != VK_SUCCESS) {
            throw std::runtime_error("failed to create window surface!");
        }

    }

	void VulkanRenderer::createSwapChain()
	{
		// Get SwapChain details so we can choose the best settings
		SwapChainDetails swapChainDetails = getSwapChainDetails(mainDevice.physicalDevice);

		VkSurfaceFormatKHR surfaceFormat = chooseBestSurfaceFormat(swapChainDetails.formats); // Choose best SURFACE FROMAT
		VkPresentModeKHR presentMode = chooseBestPresentationMode(swapChainDetails.presentationModes); // Choose best PRESENTATION MODE
		VkExtent2D extent = chooseSwapExtent(swapChainDetails.surfaceCapabilities); // Choose Swap Chain Image RESOLUTION

		// How many images are in the swap chain? Get 1 more than the minimum to allow triple buffering
		uint32_t imageCount = swapChainDetails.surfaceCapabilities.minImageCount + 1;

		// If imageCount higher than max, then clamp down to max
		// If 0, then limitless
		if (swapChainDetails.surfaceCapabilities.maxImageCount > 0
			&& swapChainDetails.surfaceCapabilities.maxImageCount < imageCount)
		{
			imageCount = swapChainDetails.surfaceCapabilities.maxImageCount;
		}

		// Creation information for swap chain
		VkSwapchainCreateInfoKHR swapChainCreateInfo = {};
		swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		swapChainCreateInfo.surface = surface;														// Swapchain surface
		swapChainCreateInfo.imageFormat = surfaceFormat.format;										// Swapchain format
		swapChainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;								// Swapchain colour space
		swapChainCreateInfo.presentMode = presentMode;												// Swapchain presentation mode
		swapChainCreateInfo.imageExtent = extent;													// Swapchain image extents
		swapChainCreateInfo.minImageCount = imageCount;												// Minimum images in swapchain
		swapChainCreateInfo.imageArrayLayers = 1;													// Number of layers for each image in chain
		// attachements image will have in the swapchain
		swapChainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;						// What attachment images will be used as
		swapChainCreateInfo.preTransform = swapChainDetails.surfaceCapabilities.currentTransform;	// Transform to perform on swap chain images
		swapChainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;						// How to handle blending images with external graphics (e.g. other windows)
		swapChainCreateInfo.clipped = VK_TRUE;														// Whether to clip parts of image not in view (e.g. behind another window, off screen, etc)

		// Get Queue Family Indices
		QueueFamilyIndices indices = getQueueFamilies(mainDevice.physicalDevice);

		// If Graphics and Presentation families are different, then swapchain must let images be shared between families
		if (indices.graphicsFamily != indices.presentationFamily)
		{
			// Queues to share between
			uint32_t queueFamilyIndices[] = {
				(uint32_t)indices.graphicsFamily,
				(uint32_t)indices.presentationFamily
			};

			swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;		// Image share handling
			swapChainCreateInfo.queueFamilyIndexCount = 2;							// Number of queues to share images between
			swapChainCreateInfo.pQueueFamilyIndices = queueFamilyIndices;			// Array of queues to share between
		}
		else
		{
			swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			swapChainCreateInfo.queueFamilyIndexCount = 0;
			swapChainCreateInfo.pQueueFamilyIndices = nullptr;
		}

		// IF old swap chain been destroyed and this one replaces it, then link old one to quickly hand over responsibilities
		swapChainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

		// Create Swapchain
		VkResult result = vkCreateSwapchainKHR(mainDevice.logicalDevice, &swapChainCreateInfo, nullptr, &swapchain);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create a Swapchain!");
		}

		// Store for later reference
		swapChainImageFormat = surfaceFormat.format;
		swapChainExtent = extent;

		// Get swap chain images(first count, then values)
		uint32_t swapChainImageCount;
		vkGetSwapchainImagesKHR(mainDevice.logicalDevice, swapchain, &swapChainImageCount, nullptr);
		std::vector<VkImage> images(swapChainImageCount);
		vkGetSwapchainImagesKHR(mainDevice.logicalDevice, swapchain, &swapChainImageCount, images.data());

		for (VkImage image : images)
		{
			// Store image handle
			SwapchainImage swapChainImage = {};
			swapChainImage.image = image;
			swapChainImage.imageView = createImageView(image, swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT); // what aspect of the image we want to view

			// Add to swapchain image list
			swapChainImages.push_back(swapChainImage);
		}
	}

	void VulkanRenderer::createRenderPass()
	{
		// Colour attachment of render pass
		VkAttachmentDescription colourAttachment = {};
		colourAttachment.format = swapChainImageFormat;						// Format of attachment (should match swapchain)
		colourAttachment.samples = VK_SAMPLE_COUNT_1_BIT;					// Number of samples to write for multisampling (1 = no multisampling)
		colourAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;				// What to do with attachment before rendering (clear it)
		colourAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;				// What to do with attachment after rendering (store it)
		colourAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;	// What to do with stencil data before rendering (not using stencil, so don't care)
		colourAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;	// What to do with stencil data after rendering (not using stencil, so don't care)
		// Frambuffer data will be stored as an image, but images can be of different data layouts
		colourAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;			// Layout of attachment before render pass (undefined)
		// SubpassLayout
		colourAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;		// Layout of attachment after render pass (ready for presentation)


		// Attachment reference uses an attachment index that refers to attachment index in the attachement list passed to renderPassCreateInfo
		VkAttachmentReference colorAttachmentRef = {};						
		colorAttachmentRef.attachment = 0;											// Index of attachment in array (which attachment to reference)
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;		// Layout to use during subpass (optimal for colour attachment)

		// Information about a particular subpass the render pass will use
		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;			// Type of subpass (graphics or compute)
		subpass.colorAttachmentCount = 1;										// Number of color attachments
		subpass.pColorAttachments = &colorAttachmentRef;						// List of color attachments references


		// Need to determine when layout transitions occur using (Subpass dependencies) --------------------------------------
		std::array<VkSubpassDependency, 2> subpassDependencies;

		// --> Transition from (VK_IMAGE_LAYOUT_UNDEFINED) -> (VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
		// -> Transition must happen after...
		subpassDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;							// subpass before(outside) render pass
		subpassDependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;			// Pipeline stage to wait on
		subpassDependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;					// Type of memory access to wait on

		// -> But must happen before...
		subpassDependencies[0].dstSubpass = 0;												// Our subpass destination
		subpassDependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; // Pipeline stage to wait on
		subpassDependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT; // Type of memory access to wait on
		subpassDependencies[1].dependencyFlags = 0;				// Framebuffer-local dependency

		// --> Transition from (VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) -> (VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
		// -> Transition must happen after...
		subpassDependencies[1].srcSubpass = 0;												// Our subpass before
		subpassDependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; // Pipeline stage to wait on
		subpassDependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT; // Type of memory access to wait on

		// -> But must happen before...
		subpassDependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;							// subpass after(outside) render pass
		subpassDependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;			// Pipeline stage to wait on
		subpassDependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;					// Type of memory access to wait on
		subpassDependencies[1].dependencyFlags = 0;

		// Create render pass ------------------------------------------------------------------------------------------------
		VkRenderPassCreateInfo renderPassCreateInfo = {};
		renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		// Attachments describe the number of color and depth buffers there are, and how to handle them
		renderPassCreateInfo.attachmentCount = 1;							// Number of attachments
		renderPassCreateInfo.pAttachments = &colourAttachment;				// Pointer to array of attachments
		renderPassCreateInfo.subpassCount = 1;								// Number of subpasses
		renderPassCreateInfo.pSubpasses = &subpass;							// Pointer to array of subpasses
		renderPassCreateInfo.dependencyCount = static_cast<uint32_t>(subpassDependencies.size()); // Number of subpass dependencies
		renderPassCreateInfo.pDependencies = subpassDependencies.data();		// Pointer to array of subpass dependencies

		VkResult result = vkCreateRenderPass(mainDevice.logicalDevice, &renderPassCreateInfo, nullptr, &renderPass);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create a Render Pass!");
		}
	}

	void VulkanRenderer::createDescriptorSetlayout()
	{
		// UboViewProjection Binding info
		VkDescriptorSetLayoutBinding vpLayoutBinding = {};
		vpLayoutBinding.binding = 0;											// Binding index in shader where data will be accessed
		vpLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;		// Type of descriptor (uniform, image sampler, etc)
		vpLayoutBinding.descriptorCount = 1;									// Number of descriptors for binding, can be more than 1 for arrays
		vpLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;				// Shader stage that will access this binding
		vpLayoutBinding.pImmutableSamplers = nullptr;							// For Texture: Used for image sampling, not used for UBOs

		// Model Binding info
		/*
		VkDescriptorSetLayoutBinding modelLayoutBinding = {};
		modelLayoutBinding.binding = 1;														// Binding index in shader where data will be accessed
		modelLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;		// Type of descriptor (uniform, image sampler, etc)
		modelLayoutBinding.descriptorCount = 1;												// Number of descriptors for binding, can be more than 1 for arrays
		modelLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;							// Shader stage that will access this binding
		modelLayoutBinding.pImmutableSamplers = nullptr;									// For Texture: Used for image sampling, not used for UBOs
		*/
		std::vector<VkDescriptorSetLayoutBinding> layoutBindings = { vpLayoutBinding };

		// Create Descriptor set layout with given bindings
		VkDescriptorSetLayoutCreateInfo layoutCreateInfo = {};
		layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutCreateInfo.bindingCount = static_cast<uint32_t>(layoutBindings.size());		// Number of bindings in layout
		layoutCreateInfo.pBindings = layoutBindings.data();										// Pointer to array of bindings

		// Create Descriptor Set Layout
		VkResult result = vkCreateDescriptorSetLayout(mainDevice.logicalDevice, &layoutCreateInfo, nullptr, &descriptorSetLayout);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create Descriptor Set Layout!");
		}
	}

	void VulkanRenderer::createPushConstantRange()
	{
		// Define push constant range no create info needed as push constant range is part of pipeline layout
		pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT; // Stages that will access the push constant
		pushConstantRange.offset = 0;								// Offset into the push constant data
		pushConstantRange.size = sizeof(Model);			// Size of the push constant data
	}

	void VulkanRenderer::createGraphicsPipeline()
	{
		auto vertexShaderCode = readFile("./Shaders/shader.vert.spv");
		auto fragmentShaderCode = readFile("./Shaders/shader.frag.spv");

		// Build shader modules to link to graphics pipeline
		VkShaderModule vertexShaderModule = createShaderModule(vertexShaderCode);
		VkShaderModule fragmentShaderModule = createShaderModule(fragmentShaderCode);

		// Vertex shader stage creation info
		VkPipelineShaderStageCreateInfo vertexShaderStageCreateInfo = {};
		vertexShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertexShaderStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT; // Stage this shader will be used in
		vertexShaderStageCreateInfo.module = vertexShaderModule; // Shader module to be used by stage
		vertexShaderStageCreateInfo.pName = "main"; // Name of entry point function in shader

		// Fragment shader stage creation info
		VkPipelineShaderStageCreateInfo fragmentShaderStageCreateInfo = {};
		fragmentShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragmentShaderStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT; // Stage this shader will be used in
		fragmentShaderStageCreateInfo.module = fragmentShaderModule; // Shader module to be used by stage
		fragmentShaderStageCreateInfo.pName = "main"; // Name of entry point function in shader

		// Graphics pipeline shader stages require an array of all stages being used
		VkPipelineShaderStageCreateInfo shaderStages[] = { vertexShaderStageCreateInfo, fragmentShaderStageCreateInfo };


		// Create Pipeline --------------------------------------------------------------------------------------------
		// Vertex shader stage creation info

		// - VERTEX INPUT ------------------------------------------------
		// How the data for a single vertex (including info such as position colour, texture coords, normals, etc) is as a whole
		VkVertexInputBindingDescription bindingDescription = {};
		bindingDescription.binding = 0;									// Can bind multiple streams of Data, this defines which one 
		bindingDescription.stride = sizeof(Vertex);						// Size of single vertex object
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;		// Whether to advance on every vertex or every instance

		std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions = {};

		// Position Attribute
		attributeDescriptions[0].binding = 0;								// Which binding the data comes from (should match above)
		attributeDescriptions[0].location = 0;								// Location directive of input in vertex shader
		attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;		// Format of data (also helps define size of data)
		attributeDescriptions[0].offset = offsetof(Vertex, pos);			// Offset of attribute in vertex struct

		// Color Attribute
		attributeDescriptions[1].binding = 0;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[1].offset = offsetof(Vertex, col);


		// Tells Vulkan how to interpret the raw bytes in your vertex buffers when building a graphics pipeline.
		/*
		How is each vertex structured in memory ?
		(binding descriptions)

		Which parts of the vertex go to which shader inputs ?
		(attribute descriptions)
		*/
		VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo = {};
		vertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputCreateInfo.vertexBindingDescriptionCount = 1;
		vertexInputCreateInfo.pVertexBindingDescriptions = &bindingDescription;  // List of vertex binding descriptions (data spacing between vertices and whether the data is per-vertex or per-instance)
		vertexInputCreateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
		vertexInputCreateInfo.pVertexAttributeDescriptions = attributeDescriptions.data();  // List of vertex attribute descriptions (type of attributes passed to vertex shader, which binding to load them from and at which offset)

		// - INPUT Assembly ------------------------------------------------
		VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssembly.primitiveRestartEnable = VK_FALSE;		// If you are using TRIANGLE_STRIP or LINE_STRIP, you could turn this on:

		// - VIEWPORT AND SCISSOR ------------------------------------------------
		VkViewport viewport = {};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = static_cast<float>(swapChainExtent.width);
		viewport.height = static_cast<float>(swapChainExtent.height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		VkRect2D scissor = {};
		scissor.offset = { 0, 0 };
		scissor.extent = swapChainExtent;


		VkPipelineViewportStateCreateInfo viewportState = {};
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.pViewports = &viewport;
		viewportState.scissorCount = 1;
		viewportState.pScissors = &scissor;	

		// - DYNAMIC STATE ------------------------------------------------
		//

		// - RASTERIZATION ------------------------------------------------
		VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = {};
		rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;			// Requires enabling GPU feature: depthClamp
		rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
		rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizationStateCreateInfo.lineWidth = 1.0f;						// Most GPUs only support 1.0f.
		rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;   // Vertex order for front faces
		rasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;					// Whether to use depth biasing (good for decals, etc)

		// - MULTISAMPLING ------------------------------------------------
		VkPipelineMultisampleStateCreateInfo multiSamplingCreateInfo = {};
		multiSamplingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multiSamplingCreateInfo.sampleShadingEnable = VK_FALSE;					// Enable multisample shading or not
		multiSamplingCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;	// Number of samples per pixelz

		// - COLOR BLENDING ------------------------------------------------
		//Blend Attachment state (how blending is handled)
		VkPipelineColorBlendAttachmentState colourState = {};
		colourState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;  // Colours to apply blending to
		colourState.blendEnable = VK_TRUE;				// Enable Color blending
		// Blend Equation => FinalColor = (SrcColor * SrcFactor) <BlendOp> (DstColor * DstFactor)
		// alpha represents how transparent the new (source) fragment is.
		colourState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;				// SrcFactor
		colourState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;		// DstFactor
		colourState.colorBlendOp = VK_BLEND_OP_ADD;									// BlendOp

		colourState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;				// SrcFactor (for alpha channel)
		colourState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;				// DstFactor (for alpha channel)
		colourState.alphaBlendOp = VK_BLEND_OP_ADD;							// BlendOp (for alpha channel)
		// Summarized: (1 * newAlpha) + (0 * oldAlpha) = newAlpha


		VkPipelineColorBlendStateCreateInfo colorBlendingCreateInfo = {};
		colorBlendingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlendingCreateInfo.logicOpEnable = VK_FALSE;							// Whether to use logic operations when writing to frame buffer
		//colorBlendingCreateInfo.logicOp = VK_LOGIC_OP_COPY;						// Type of logic operation to use
		colorBlendingCreateInfo.attachmentCount = 1;									// Number of frame buffers to create blending for
		colorBlendingCreateInfo.pAttachments = &colourState;						// Pointer to array of blend attachment states



		// - PIPELINE LAYOUT ------------------------------------------------
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
		pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutCreateInfo.setLayoutCount = 1;
		pipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayout;
		pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
		pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;

		// Create Pipeline Layouts
		VkResult result = vkCreatePipelineLayout(mainDevice.logicalDevice, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create Pipeline Layout!");
		}

		// TODO: Set up depth and stencil testing


		// - FINAL GRAPHICS PIPELINE CREATION ----------------------------------------------------------------------------------------------------
		VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = {};
		graphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		graphicsPipelineCreateInfo.stageCount = 2;											// Number of shader stages
		graphicsPipelineCreateInfo.pStages = shaderStages;									// Pointer to array of shader stages
		graphicsPipelineCreateInfo.pVertexInputState = &vertexInputCreateInfo;				// Vertex input state info
		graphicsPipelineCreateInfo.pInputAssemblyState = &inputAssembly;					// Input assembly state info
		graphicsPipelineCreateInfo.pViewportState = &viewportState;							// Viewport state info
		graphicsPipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;		// Rasterization state info
		graphicsPipelineCreateInfo.pMultisampleState = &multiSamplingCreateInfo;			// Multisampling state info
		graphicsPipelineCreateInfo.pColorBlendState = &colorBlendingCreateInfo;			// Color blending state info
		graphicsPipelineCreateInfo.pDepthStencilState = nullptr;						// Depth and stencil state info
		graphicsPipelineCreateInfo.layout = pipelineLayout;								// Pipeline layout used by pipeline
		graphicsPipelineCreateInfo.renderPass = renderPass;								// render pass description the pipeline is compatible with
		graphicsPipelineCreateInfo.subpass = 0;											// Subpass index of render pass where this pipeline will be used
		graphicsPipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;					// Handle to base pipeline if deriving from existing one
		graphicsPipelineCreateInfo.basePipelineIndex = -1;								// Index of base pipeline in pCreateInfos array if creating multiple pipelines

		result = vkCreateGraphicsPipelines(mainDevice.logicalDevice, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo, nullptr, &graphicsPipeline);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create Graphics Pipeline!");
		}
		// ---------------------------------------------- END --------------------------------------------------------------------------------------

		// Destory Shader module -> not needed after pipeline creation
		vkDestroyShaderModule(mainDevice.logicalDevice, fragmentShaderModule, nullptr);
		vkDestroyShaderModule(mainDevice.logicalDevice, vertexShaderModule, nullptr);
	}

	void VulkanRenderer::createFramebuffers()
	{
		swapChainFramebuffers.resize(swapChainImages.size());

		// Create a FrameBuffer for each swap chain image view
		for (size_t i = 0; i < swapChainFramebuffers.size(); i++)
		{
			std::array<VkImageView, 1> attachments = {
				swapChainImages[i].imageView
			};
			VkFramebufferCreateInfo framebufferCreateInfo = {};
			framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferCreateInfo.renderPass = renderPass;											// Render pass layout the framebuffer will be used with
			framebufferCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
			framebufferCreateInfo.pAttachments = attachments.data();								// List of attachments (1:1 with render pass)
			framebufferCreateInfo.width = swapChainExtent.width;
			framebufferCreateInfo.height = swapChainExtent.height;
			framebufferCreateInfo.layers = 1;
			if (vkCreateFramebuffer(mainDevice.logicalDevice, &framebufferCreateInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS)
			{
				throw std::runtime_error("Failed to create framebuffer!");
			}
		}
	}

	void VulkanRenderer::createCommandPool()
	{
		QueueFamilyIndices queueFamilyIndices = getQueueFamilies(mainDevice.physicalDevice);

		VkCommandPoolCreateInfo commandPoolCreateInfo = {};
		commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;  // Allow command buffers to be rerecorded individually
		commandPoolCreateInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily;		// Queue family type that command buffers from this pool will use
		commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;	// Allow command buffers to be rerecorded individually
		if (vkCreateCommandPool(mainDevice.logicalDevice, &commandPoolCreateInfo, nullptr, &graphicsCommandPool) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create command pool!");
		}
	}

	void VulkanRenderer::createCommandBuffers()
	{
		commandBuffers.resize(swapChainFramebuffers.size());

		VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
		commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		commandBufferAllocateInfo.commandPool = graphicsCommandPool;						// Command pool to allocate from (Command buffer now is sealed to work with only graphics queue families)
		commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;					// Level of command buffers (primary or secondary)
		//commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;				// Secondary can be called from primary: Buffer directly callable by queue
		commandBufferAllocateInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size()); // Number of command buffers to allocate
		
		// Allocate command buffers
		if (vkAllocateCommandBuffers(mainDevice.logicalDevice, &commandBufferAllocateInfo, commandBuffers.data()) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to allocate command buffers!");
		}
	}

	void VulkanRenderer::createSyncObjects()
	{
		imageAvailableSemaphore.resize(MAX_FRAME_DRAWS);
		renderFinishedSemaphore.resize(MAX_FRAME_DRAWS);
		drawFences.resize(MAX_FRAME_DRAWS);

		VkSemaphoreCreateInfo semaphoreCreateInfo = {};
		semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		// Fence creation information:
		VkFenceCreateInfo fenceCreateInfo = {};
		fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // Create the fence in signaled state so we don't wait forever on first frame

		for (size_t i = 0; i < MAX_FRAME_DRAWS; i++)
		{
			if (vkCreateSemaphore(mainDevice.logicalDevice, &semaphoreCreateInfo, nullptr, &imageAvailableSemaphore[i]) != VK_SUCCESS ||
				vkCreateSemaphore(mainDevice.logicalDevice, &semaphoreCreateInfo, nullptr, &renderFinishedSemaphore[i]) != VK_SUCCESS ||
				vkCreateFence(mainDevice.logicalDevice, &fenceCreateInfo, nullptr, &drawFences[i]) != VK_SUCCESS)
			{
				throw std::runtime_error("Failed to create semaphores or fences!");
			}
		}
	}

	void VulkanRenderer::createUniformBuffers()
	{
		// ViewProjection UBO buffer size
		VkDeviceSize vpBufferSize = sizeof(UboViewProjection);

		// Model buffer size
		//VkDeviceSize modelBufferSize = modelUniformAlignment * MAX_OBJECTS; // Dynamic UBO with space for max objects

		// One uniform buffer per swapchain image (and by extension, command buffer)
		vpUniformBuffers.resize(swapChainImages.size());
		vpUniformBuffersMemory.resize(swapChainImages.size());
		/*modelDUniformBuffers.resize(swapChainImages.size());
		modelDUniformBuffersMemory.resize(swapChainImages.size());*/

		// Create the uniform buffers
		for (size_t i = 0; i < swapChainImages.size(); i++)
		{
			createBuffer(mainDevice.physicalDevice, mainDevice.logicalDevice, vpBufferSize,
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				vpUniformBuffers[i], vpUniformBuffersMemory[i]);

			/*createBuffer(mainDevice.physicalDevice, mainDevice.logicalDevice, modelBufferSize,
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				modelDUniformBuffers[i], modelDUniformBuffersMemory[i]);*/
		}


	}

	void VulkanRenderer::createDescriptorPool()
	{
		// Type of Descriptors in pool + how many of descriptor, not descriptor sets (combined makes the pool size)
		// ViewProjection Pool
		VkDescriptorPoolSize vpPoolSize = {};
		vpPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;							// Type of descriptor in pool
		vpPoolSize.descriptorCount = static_cast<uint32_t>(vpUniformBuffers.size());	// Number of descriptors in pool of that type

		// Model Pool 
		/*
		VkDescriptorPoolSize modelPoolSize = {};
		modelPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;						// Type of descriptor in pool
		modelPoolSize.descriptorCount = static_cast<uint32_t>(modelDUniformBuffers.size());	// Number of descriptors in pool of that type
		*/

		// Combine pool sizes into single array
		std::vector<VkDescriptorPoolSize> poolSizes = { vpPoolSize };

		// Data to create Descriptor pool
		VkDescriptorPoolCreateInfo poolCreateInfo = {};
		poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolCreateInfo.maxSets = static_cast<uint32_t>(swapChainImages.size());		// Maximum number of descriptor sets that can be allocated from pool
		poolCreateInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());		// Number of different descriptor types
		poolCreateInfo.pPoolSizes = poolSizes.data();								// Pointer to array of pool sizes

		// Create Descriptor Pool
		if (vkCreateDescriptorPool(mainDevice.logicalDevice, &poolCreateInfo, nullptr, &descriptorPool) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create Descriptor Pool!");
		}
	}

	void VulkanRenderer::createDescriptorSets()
	{
		// Resize descriptor sets to number of uniform buffers (1 per swapchain image)
		descriptorSets.resize(swapChainImages.size());

		std::vector<VkDescriptorSetLayout> setLayouts(swapChainImages.size(), descriptorSetLayout); // All layouts will be the same

		// Info to allocate descriptor sets from pool
		VkDescriptorSetAllocateInfo descriptorSetAllocInfo = {};
		descriptorSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descriptorSetAllocInfo.descriptorPool = descriptorPool;										// Pool to allocate from
		descriptorSetAllocInfo.descriptorSetCount = static_cast<uint32_t>(swapChainImages.size());	// Number of sets to allocate
		descriptorSetAllocInfo.pSetLayouts = setLayouts.data();										// // Layouts to use to allocate sets (1:1 relationship)a

		// Allocate descriptor sets (multiple)
		if (vkAllocateDescriptorSets(mainDevice.logicalDevice, &descriptorSetAllocInfo, descriptorSets.data()) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to allocate Descriptor Sets!");
		}

		// Configure each descriptor set to point to corresponding uniform buffer
		for (size_t i = 0; i < swapChainImages.size(); i++)
		{
			// View Projection UBO Descriptor info
			// Buffer Info and Data offset info: what buffer to bind to set, offset and range of data to bind
			VkDescriptorBufferInfo vpBufferInfo = {};
			vpBufferInfo.buffer = vpUniformBuffers[i];					// Buffer to bind to set
			vpBufferInfo.offset = 0;									// Offset in buffer to start at
			vpBufferInfo.range = sizeof(UboViewProjection);							// Size of data to bind
			// Write information into descriptor set
			VkWriteDescriptorSet vpDescriptorWrite = {};
			vpDescriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			vpDescriptorWrite.dstSet = descriptorSets[i];				// Descriptor set to update
			vpDescriptorWrite.dstBinding = 0;							// Binding in shader where data will be available
			vpDescriptorWrite.dstArrayElement = 0;						// First array element to update
			vpDescriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; // Type of descriptor
			vpDescriptorWrite.descriptorCount = 1;						// Number of descriptors to update
			vpDescriptorWrite.pBufferInfo = &vpBufferInfo;			// How this descriptor set is connecting with the buffer described above
			
			// Model Dynamic UBO Descriptor info
			/*
			VkDescriptorBufferInfo modelBufferInfo = {};
			modelBufferInfo.buffer = modelDUniformBuffers[i];					// Buffer to bind to set
			modelBufferInfo.offset = 0;										// Offset in buffer to start at
			modelBufferInfo.range = modelUniformAlignment;					// Size of data to bind (only one model at a time)
			// Write information into descriptor set
			VkWriteDescriptorSet modelDescriptorWrite = {};
			modelDescriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			modelDescriptorWrite.dstSet = descriptorSets[i];					// Descriptor set to update
			modelDescriptorWrite.dstBinding = 1;								// Binding in shader where data will be available
			modelDescriptorWrite.dstArrayElement = 0;							// First array element to update
			modelDescriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC; // Type of descriptor
			modelDescriptorWrite.descriptorCount = 1;							// Number of descriptors to update
			modelDescriptorWrite.pBufferInfo = &modelBufferInfo;				// How this descriptor set is connecting with the buffer described above
			*/
			std::vector<VkWriteDescriptorSet> writeDescriptorSets = { vpDescriptorWrite };

			// Update the descriptor set with new buffer/binding info:
			vkUpdateDescriptorSets(mainDevice.logicalDevice, static_cast<uint32_t>(writeDescriptorSets.size()),
				writeDescriptorSets.data(), 0, nullptr);
		}
	}

	void VulkanRenderer::updateUniformBuffers(uint32_t imageIndex)
	{
		
		// Map memory and write to uniform buffer
		void* data;
		vkMapMemory(mainDevice.logicalDevice, vpUniformBuffersMemory[imageIndex], 0, sizeof(UboViewProjection), 0, &data);    // Map to pointer
		memcpy(data, &uboViewProjection, sizeof(UboViewProjection));														// Copy data to mapped memory
		vkUnmapMemory(mainDevice.logicalDevice, vpUniformBuffersMemory[imageIndex]);											// Unmap from pointer

		//// Model UBOs (Dynamic)
		//for (size_t i = 0; i < meshList.size(); i++)
		//{
		//	Model* thisModel = (Model*)((uint64_t)modelTransferSpace + (i * modelUniformAlignment));
		//	*thisModel = meshList[i].getUboModel();
		//}

		//// Map the list of model data
		//vkMapMemory(mainDevice.logicalDevice, modelDUniformBuffersMemory[imageIndex], 0, modelUniformAlignment * meshList.size(), 0, &data);
		//memcpy(data, modelTransferSpace, modelUniformAlignment * meshList.size());
		//vkUnmapMemory(mainDevice.logicalDevice, modelDUniformBuffersMemory[imageIndex]);
	}

	void VulkanRenderer::recordCommand(uint32_t currentImage)
	{
		VkCommandBufferBeginInfo commandBufferBeginInfo = {};
		commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		//commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT; // Buffer can be resubmitted while it is also already pending execution

		// Information to begin a render pass (only for graphical applications)
		VkRenderPassBeginInfo renderPassBeginInfo = {};
		renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassBeginInfo.renderPass = renderPass;									// Render pass to begin
		renderPassBeginInfo.renderArea.offset = { 0, 0 };								// Offset to start render area from
		renderPassBeginInfo.renderArea.extent = swapChainExtent;						// Extent of render area to run within
		// Clear values for each attachment the render pass uses
		VkClearValue clearColour[] = {
			{0.6f, 0.65f, 0.4f, 1.0f}
		};
		renderPassBeginInfo.pClearValues = clearColour;									// Pointer to array of clear values
		renderPassBeginInfo.clearValueCount = 1;										// Number of clear values
		// TODO: Add depth clear value if using depth buffering

		// Begin recording command buffer
		if (vkBeginCommandBuffer(commandBuffers[currentImage], &commandBufferBeginInfo) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to begin recording command buffer!");
		}

		// Begin render pass
		renderPassBeginInfo.framebuffer = swapChainFramebuffers[currentImage];					// Framebuffer to use
			
		// vkCmd * commands go here -----------------------------------------------------
		vkCmdBeginRenderPass(commandBuffers[currentImage], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE); // Contents will be inline (not secondary command buffers)

			// Bind graphics pipeline to be used in the Render Pass
			vkCmdBindPipeline(commandBuffers[currentImage], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline); // Bind graphics pipeline

			for (size_t j = 0; j < meshList.size(); j++)
			{
				VkBuffer vertexBuffers = { meshList[j].getVertexBuffer()};	// Buffers to bind
				VkDeviceSize offsets[] = { 0 };								// Offsets into buffers
				vkCmdBindVertexBuffers(commandBuffers[currentImage], 0, 1, &vertexBuffers, offsets); // Bind veretex buffer to pipeline

				// Bind mesh index buffer
				vkCmdBindIndexBuffer(commandBuffers[currentImage], meshList[j].getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32); // Bind index buffer

				// Dynamic offset for model UBO
				//uint32_t dynamicOffset = static_cast<uint32_t>(modelUniformAlignment * j); // Offset into dynamic UBO for this object's model data
				// above offset will only be applied to dynamic uniform buffer bindings in the descriptor set
				
				// Bind push constants (model data)
				vkCmdPushConstants(commandBuffers[currentImage], pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, 
					sizeof(Model), &meshList[j].getUboModel());
				
				// Bind Descriptor sets (uniform buffers) to pipeline
				vkCmdBindDescriptorSets(commandBuffers[currentImage], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 
					0, 1, &descriptorSets[currentImage], 0, nullptr);

				// Issue draw command
				// Connected to GLSL gl_VertexIndex variable in vertex shader
				//vkCmdDraw(commandBuffers[i], static_cast<uint32_t>(firstMesh.getVertexCount()), 1, 0, 0); // Draw 3 vertices, 1 instance, first vertex 0, first instance 0
				vkCmdDrawIndexed(commandBuffers[currentImage], static_cast<uint32_t>(meshList[j].getIndexCount()), 1, 0, 0, 0); // Draw indexed

			}

		vkCmdEndRenderPass(commandBuffers[currentImage]); // End render pass
			
		// End recording command buffer
		VkResult result = vkEndCommandBuffer(commandBuffers[currentImage]);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to record command buffer!");
		}
	}

	void VulkanRenderer::getPhysicalDevice()
	{
		// Enumerate physical devices that VkInstance can access
		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices(_instance, &deviceCount, nullptr);

		if (deviceCount == 0) {
			throw std::runtime_error("failed to find GPUs with Vulkan support!");
		}

		std::vector<VkPhysicalDevice> devicesList(deviceCount);
		vkEnumeratePhysicalDevices(_instance, &deviceCount, devicesList.data());

		for (const auto& device : devicesList)
		{
			if (checkDeviceSuitable(device))
			{
				mainDevice.physicalDevice = device;
				break;
			}
		}

		// Get properties of selected device
		VkPhysicalDeviceProperties deviceProperties;
		vkGetPhysicalDeviceProperties(mainDevice.physicalDevice, &deviceProperties);
		std::cout << "Selected GPU: " << deviceProperties.deviceName << std::endl;

		//minUniformBufferOffset = deviceProperties.limits.minUniformBufferOffsetAlignment;
	}

	void VulkanRenderer::allocateDynamicBufferTransferSpace()
	{
		// Calculate alignment of model data
		//modelUniformAlignment = (sizeof(Model) + minUniformBufferOffset - 1) & ~(minUniformBufferOffset - 1);

		//// Create Space in memory to hold dynamic buffer that is aligned properly to required alignment and can hold MAX_OBJECTS
		//modelTransferSpace = (Model*)_aligned_malloc(modelUniformAlignment * MAX_OBJECTS, modelUniformAlignment);

		
	}
	

	bool VulkanRenderer::checkInstanceExtensionSupport(const std::vector<const char*>* checkExtensions)
	{
		// Get number of extensions supported to create the array of correct size
		uint32_t extensionCount = 0;  
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

		// Get list of supported extensions
		std::vector<VkExtensionProperties> availableExtensions(extensionCount);
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableExtensions.data());

		// Check if all extensions in checkExtensions are in the list of available extensions
		for (const auto& checkExtension : *checkExtensions)
		{
			bool hasExtension = false;
			for (const auto& availableExtension : availableExtensions)
			{
				if (strcmp(checkExtension, availableExtension.extensionName))
				{
					hasExtension = true;
					break;
				}
			}

			if (!hasExtension)
			{
				return false;
			}
		}

		return true;
	}

	bool VulkanRenderer::checkDeviceExtensionSupport(VkPhysicalDevice phyDevice)
	{
		uint32_t extensionCount = 0;
		vkEnumerateDeviceExtensionProperties(phyDevice, nullptr, &extensionCount, nullptr);

		if (extensionCount == 0) { return false; }

		std::vector<VkExtensionProperties> extensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(phyDevice, nullptr, &extensionCount, extensions.data());

		for (const auto& deviceExtension : deviceExtensions)
		{
			bool hasExtension = false;
			for (const auto& extension : extensions)
			{
				if (strcmp(deviceExtension, extension.extensionName) == 0)
				{
					hasExtension = true;
					break;
				}
			}

			if (!hasExtension) return false;
		}
		return true;
	}

	bool VulkanRenderer::checkDeviceSuitable(VkPhysicalDevice device)
	{
		/*
		// Information about the device itself (ID, name, type, etc.)
		VkPhysicalDeviceProperties deviceProperties;
		vkGetPhysicalDeviceProperties(device, &deviceProperties);

		// Features (geometry shaders, tessellation shaders, etc.)
		VkPhysicalDeviceFeatures deviceFeatures;
		vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
		*/

		QueueFamilyIndices indices = getQueueFamilies(device); // Queues are checked here
		
		bool extensionSupported = checkDeviceExtensionSupport(device);

		bool swapChainValid = false;
		if (extensionSupported)
		{
			SwapChainDetails swapChainDetails = getSwapChainDetails(device);
			swapChainValid = !swapChainDetails.presentationModes.empty() && !swapChainDetails.formats.empty();
		}

		return indices.isValid() && extensionSupported && swapChainValid;
	}


	QueueFamilyIndices VulkanRenderer::getQueueFamilies(VkPhysicalDevice device)
	{
		QueueFamilyIndices indices;

		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties> queueFamilyList(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilyList.data());

		// Go through each queue family and check if it has at least 1 required types of queue
		int i = 0;
		for (const auto& queueFamily : queueFamilyList)
		{
			if (queueFamily.queueCount > 0 && queueFamily.queueFlags && VK_QUEUE_GRAPHICS_BIT)
			{
				indices.graphicsFamily = i; // If queue family has graphics capability, set its index
			}

			// check if Queue family supports presentation
			VkBool32 presentationSupport = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentationSupport);
			// If queue is presentation type (can be both graphics and presentation)
			if (queueFamily.queueCount > 0 && presentationSupport)
			{
				indices.presentationFamily = i; // If queue family has presentation capability, set its index
			}

			if (indices.isValid())
			{
				break; // If all required queue families are found, break out of loop
			}

			i++;
		}

		return indices;
	}

	SwapChainDetails VulkanRenderer::getSwapChainDetails(VkPhysicalDevice device)
	{
		SwapChainDetails swapChainDetails;

		// - Capabilities -
		// Get the suface capabilities for the given surface on the given physical device
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &swapChainDetails.surfaceCapabilities);

		// - Formats returned, get list of format -
		uint32_t formatCount = 0;
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

		if (formatCount != 0)
		{
			swapChainDetails.formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, swapChainDetails.formats.data());
		}

		// - Presentation Mode -
		uint32_t presentationCount = 0;
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentationCount, nullptr);

		if (presentationCount != 0)
		{
			swapChainDetails.presentationModes.resize(presentationCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentationCount, swapChainDetails.presentationModes.data());
		}

		return swapChainDetails;
	}

	// Best format is subjective, ours will be
	// Format : VK_FORMAT_R8G8B8A8_UNORM
	// ColorSpace : VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
	VkSurfaceFormatKHR VulkanRenderer::chooseBestSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats)
	{
		// If only 1 format available and is undefined, then this means ALL formats are available (no restrictions)
		if (formats.size() == 1 && formats[0].format == VK_FORMAT_UNDEFINED) {
			return { VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
		}

		for (const auto& format : formats)
		{
			if (format.format == VK_FORMAT_R8G8B8A8_UNORM || format.format == VK_FORMAT_B8G8R8A8_UNORM 
				&& format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			{
				return format;
			}
		}

		return formats[0];
	}

	VkPresentModeKHR VulkanRenderer::chooseBestPresentationMode(const std::vector<VkPresentModeKHR>& presentationModes)
	{
		// Look for mailbox presentation mode
		for (const auto& presentationMode : presentationModes)
		{
			if (presentationMode == VK_PRESENT_MODE_MAILBOX_KHR)
			{
				return presentationMode;
			}
		}

		// If can't find, use FIFO as Vulkan spec says it must be present
		return VK_PRESENT_MODE_FIFO_KHR;
	}

	VkExtent2D VulkanRenderer::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities)
	{
		// If current extent is at numeric limits, then extent can vary. Otherwise, it is the size of the window.
		if (surfaceCapabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
		{
			return surfaceCapabilities.currentExtent;
		}
		else
		{
			// If value can vary, need to set manually

			// Get window size
			int width, height;
			glfwGetFramebufferSize(_window, &width, &height);

			// Create new extent using window size
			VkExtent2D newExtent = {};
			newExtent.width = static_cast<uint32_t>(width);
			newExtent.height = static_cast<uint32_t>(height);

			// Surface also defines max and min, so make sure within boundaries by clamping value
			newExtent.width = std::max(surfaceCapabilities.minImageExtent.width, std::min(surfaceCapabilities.maxImageExtent.width, newExtent.width));
			newExtent.height = std::max(surfaceCapabilities.minImageExtent.height, std::min(surfaceCapabilities.maxImageExtent.height, newExtent.height));

			return newExtent;
		}
	}

	VkImageView VulkanRenderer::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags)
	{
		VkImageViewCreateInfo viewCreateInfo = {};
		viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewCreateInfo.image = image;											// Image to create view for
		viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;						// Type of image (1D, 2D, 3D, Cube, etc)
		viewCreateInfo.format = format;											// Format of image data
		viewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;			// Allows remapping of rgba components to other rgba values
		viewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

		// Subresources allow the view to view only a part of an image
		viewCreateInfo.subresourceRange.aspectMask = aspectFlags;				// Which aspect of image to view (e.g. COLOR_BIT for viewing colour)
		viewCreateInfo.subresourceRange.baseMipLevel = 0;						// Start mipmap level to view from
		viewCreateInfo.subresourceRange.levelCount = 1;							// Number of mipmap levels to view
		viewCreateInfo.subresourceRange.baseArrayLayer = 0;						// Start array level to view from
		viewCreateInfo.subresourceRange.layerCount = 1;							// Number of array levels to view

		// Create image view and return it
	VkImageView imageView;
	VkResult result = vkCreateImageView(mainDevice.logicalDevice, &viewCreateInfo, nullptr, &imageView);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create an Image View!");
	}

	return imageView;
	}

	VkShaderModule VulkanRenderer::createShaderModule(const std::vector<char>& code)
	{
		VkShaderModuleCreateInfo shaderModuleCreateInfo = {};
		shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		shaderModuleCreateInfo.codeSize = code.size(); // Size of code in bytes
		shaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(code.data()); // Pointer to code (bytecode of shader)

		VkShaderModule shaderModule;
		VkResult result = vkCreateShaderModule(mainDevice.logicalDevice, &shaderModuleCreateInfo, nullptr, &shaderModule);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create shader module!");
		}

		return shaderModule;
	}
}