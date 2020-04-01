#include "Common.hpp"
#include "Application.hpp"
#include "IO.hpp"

namespace Kumo {

    static const std::vector<const char*> ValidationLayers {
        "VK_LAYER_KHRONOS_validation"
    };

    static const std::vector<const char*> DeviceExtensions {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    Application::~Application() {

    }

    void Application::Run() {
        InitializeWindow();
        InitializeVulkan();
        RunLoop();
        Cleanup();
    }

    void Application::InitializeWindow() {
        const int init_success = glfwInit();
        if (!init_success)
            throw std::runtime_error("Failed to initialize GLFW.");

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE,  GLFW_TRUE);

        m_window = glfwCreateWindow(
            static_cast<int>(WindowWidth),
            static_cast<int>(WindowHeight),
            "Vulkan",
            nullptr,
            nullptr
        );

        if (!m_window)
            throw std::runtime_error("Failed to create window.");
        glfwSetWindowUserPointer(m_window, this);
        glfwSetFramebufferSizeCallback(m_window, GLFWFramebufferResizeCallback);
    }

    void Application::InitializeVulkan() {
        KUMO_DEBUG_ONLY m_debug_messenger_create_info = {
            VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            nullptr,
            0,
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
                | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
                | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
            VulkanDebugCallback,
            nullptr
        };
        CreateInstance();
        KUMO_DEBUG_ONLY SetupDebugMessenger();
        CreateSurface();
        SelectPhysicalDevice();
        CreateLogicalDevice();
        CreateSwapchain();
        CreateSwapchainImageViews();
        CreateRenderPass();
        CreateGraphicsPipeline();
        CreateFramebuffers();
        CreateCommandPool();
        CreateCommandBuffers();
        CreateSynchronizationObjects();
    }

    void Application::RunLoop() {
        while (!glfwWindowShouldClose(m_window)) {
            glfwPollEvents();
            DrawFrame();
        }
        vkDeviceWaitIdle(m_device);
    }

    void Application::Cleanup() {
        CleanupSwapchain();
        for (size_t i = 0; i < MaxFramesInFlight; i++) {
            vkDestroyFence(m_device, m_fens_in_flight[i], nullptr);
            vkDestroySemaphore(m_device, m_sems_render_finished[i], nullptr);
            vkDestroySemaphore(m_device, m_sems_image_available[i], nullptr);
        }
        vkDestroyCommandPool(m_device, m_cmd_pool, nullptr);
        vkDestroyDevice(m_device, nullptr);
        KUMO_DEBUG_ONLY {
            const auto vkDestroyDebugUtilsMessengerEXT =
                reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                    vkGetInstanceProcAddr(
                        m_instance,
                        "vkDestroyDebugUtilsMessengerEXT"
                    )
                );
            if (vkDestroyDebugUtilsMessengerEXT) {
                vkDestroyDebugUtilsMessengerEXT(m_instance, m_debug_messenger,
                    nullptr);
            }
        }
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
        vkDestroyInstance(m_instance, nullptr);
        glfwDestroyWindow(m_window);
        glfwTerminate();
    }

    void Application::DrawFrame() {
        vkWaitForFences(m_device, 1, &m_fens_in_flight[m_current_frame],
            VK_TRUE, std::numeric_limits<UInt64>::max());

        UInt32 image_index;
        const VkResult acquisition_result = vkAcquireNextImageKHR(
            m_device,
            m_swapchain,
            std::numeric_limits<UInt64>::max(),
            m_sems_image_available[m_current_frame],
            VK_NULL_HANDLE,
            &image_index
        );

        switch (acquisition_result) {
        case VK_ERROR_OUT_OF_DATE_KHR:
            RecreateSwapchain();
            return;
        case VK_SUCCESS:
        case VK_SUBOPTIMAL_KHR:
            break;
        default:
            throw std::runtime_error("Failed to acquire swapchain image.");
        }

        if (m_fens_images_in_flight[image_index] != VK_NULL_HANDLE) {
            vkWaitForFences(m_device, 1, &m_fens_images_in_flight[image_index],
                VK_TRUE, std::numeric_limits<UInt64>::max());
        }

        const VkPipelineStageFlags wait_stages =
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        const VkSubmitInfo submit_info {
            VK_STRUCTURE_TYPE_SUBMIT_INFO,
            nullptr,
            1,
            &m_sems_image_available[m_current_frame],
            &wait_stages,
            1,
            &m_cmd_buffers[image_index],
            1,
            &m_sems_render_finished[m_current_frame]
        };

        vkResetFences(m_device, 1, &m_fens_in_flight[m_current_frame]);
        
        if (vkQueueSubmit(m_graphics_queue, 1, &submit_info,
                m_fens_in_flight[m_current_frame]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to submit draw command buffer.");
        }
        const VkPresentInfoKHR present_info {
            VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            nullptr,
            1,
            &m_sems_render_finished[m_current_frame],
            1,
            &m_swapchain,
            &image_index,
            nullptr
        };
        const VkResult present_result =
            vkQueuePresentKHR(m_present_queue, &present_info);
        if (m_framebuffer_resized
                || present_result == VK_ERROR_OUT_OF_DATE_KHR
                || present_result == VK_SUBOPTIMAL_KHR) {
            m_framebuffer_resized = false;
            RecreateSwapchain();
        } else if (present_result != VK_SUCCESS) {
            throw std::runtime_error("Failed to present swapchain image.");
        }
        m_current_frame = (m_current_frame + 1) % MaxFramesInFlight;
    }

    void Application::CreateInstance() {
        const VkApplicationInfo app_info {
            VK_STRUCTURE_TYPE_APPLICATION_INFO,
            nullptr,
            "VKTut",
            VK_MAKE_VERSION(0, 0, 1),
            "VKTut",
            VK_MAKE_VERSION(0, 0, 1),
            VK_API_VERSION_1_1
        };

        const auto required_extensions = GetRequiredExtensions();

        UInt32 n_available_extensions = 0;
        vkEnumerateInstanceExtensionProperties(nullptr,
            &n_available_extensions, nullptr);
        std::vector<VkExtensionProperties> available_extensions(
            n_available_extensions);
        vkEnumerateInstanceExtensionProperties(nullptr,
            &n_available_extensions, available_extensions.data());
        std::cout << "Available extensions:" << std::endl;
        for (const auto& extension : available_extensions)
            std::cout << "\t" << extension.extensionName << std::endl;

        for (const char* extension_name : required_extensions) {
            const auto r = std::find_if(
                available_extensions.begin(),
                available_extensions.end(),
                [extension_name] (VkExtensionProperties extension) {
                    return strcmp(extension_name, extension.extensionName) == 0;
                }
            );
            if (r == available_extensions.end()) {
                throw std::runtime_error(
                    std::string("Required Vulkan extension \"")
                    + extension_name
                    + "\" is not supported."
                );
            }
        }

        std::vector<const char*> layers;
        void* p_next = nullptr;

        UInt32 n_available_layers;
        vkEnumerateInstanceLayerProperties(&n_available_layers, nullptr);
        std::vector<VkLayerProperties> available_layers(n_available_layers);
        vkEnumerateInstanceLayerProperties(&n_available_layers,
            available_layers.data());
        std::cout << "Available layers:" << std::endl;
        for (const auto& layer : available_layers)
            std::cout << "\t" << layer.layerName << std::endl;

        KUMO_DEBUG_ONLY {
            if (!AreLayersSupported(ValidationLayers)) {
                throw std::runtime_error(
                    "Validation layers are not available."
                );
            }
            layers.insert(layers.end(), ValidationLayers.begin(),
                ValidationLayers.end());

            p_next = &m_debug_messenger_create_info;
        }

        std::cout << "Using extensions: " << std::endl;
        for (const char* extension_name : required_extensions) {
            std::cout << "\t" << extension_name << std::endl;
        }

        std::cout << "Using layers: " << std::endl;
        for (const char* layer_name : layers) {
            std::cout << "\t " << layer_name << std::endl;
        }

        const VkInstanceCreateInfo create_info {
            VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            p_next,
            0,
            &app_info,
            static_cast<UInt32>(layers.size()),
            layers.data(),
            static_cast<UInt32>(required_extensions.size()),
            required_extensions.data()
        };

        if (vkCreateInstance(&create_info, nullptr, &m_instance) != VK_SUCCESS)
            throw std::runtime_error("Failed to create Vulkan instance.");
    }

    void Application::CreateSurface() {
        if (glfwCreateWindowSurface(m_instance, m_window, nullptr,
                &m_surface) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create window surface.");
        }
    }

    void Application::SelectPhysicalDevice() {
        UInt32 n_devices;
        vkEnumeratePhysicalDevices(m_instance, &n_devices, nullptr);
        if (n_devices == 0)
            throw std::runtime_error("Found no GPU with Vulkan support.");
        std::vector<VkPhysicalDevice> devices(n_devices);
        vkEnumeratePhysicalDevices(m_instance, &n_devices, devices.data());
        std::optional<VkPhysicalDeviceProperties> device_properties =
            std::nullopt;
        std::cout << "Available devices:" << std::endl;
        for (const auto& device : devices) {
            VkPhysicalDeviceProperties properties;
            VkPhysicalDeviceFeatures   features;
            const bool suitable = IsPhysicalDeviceSuitable(device, properties, features);
            std::cout << "\t" << properties.deviceName << std::endl;
            if (!device_properties && suitable) {
                m_physical_device = device;
                device_properties = properties;
                break;
            }
        }
        if (!device_properties)
            throw std::runtime_error("Found no suitable GPU.");

        std::cout << "Using device:" << std::endl
            << "\t" << device_properties->deviceName << std::endl;
    }

    void Application::CreateLogicalDevice() {
        m_queue_family_indices = FindQueueFamilies(m_physical_device);
        const float queue_priorities[] { 1.0f };
        std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
        std::set<UInt32> unique_families {
            m_queue_family_indices.GraphicsFamily.value(),
            m_queue_family_indices.PresentFamily.value()
        };
        for (UInt32 family_index : unique_families) {
            const VkDeviceQueueCreateInfo queue_create_info {
                VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                nullptr,
                0,
                family_index,
                1,
                queue_priorities
            };
            queue_create_infos.push_back(queue_create_info);
        }
        const VkPhysicalDeviceFeatures features { };
        std::vector<const char*> layers;
        KUMO_DEBUG_ONLY {
            // Validation layers are assumed to be available
            // as their existance has been checked during instance
            // creation
            layers.insert(layers.end(), ValidationLayers.begin(),
                ValidationLayers.end());
        }
        const VkDeviceCreateInfo device_create_info {
            VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            nullptr,
            0,
            static_cast<UInt32>(queue_create_infos.size()),
            queue_create_infos.data(),
            static_cast<UInt32>(layers.size()),
            layers.data(),
            static_cast<UInt32>(DeviceExtensions.size()),
            DeviceExtensions.data(),
            &features
        };
        if (vkCreateDevice(m_physical_device, &device_create_info, nullptr,
                &m_device) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create logical device.");
        }
        vkGetDeviceQueue(
            m_device,
            m_queue_family_indices.GraphicsFamily.value(),
            0,
            &m_graphics_queue
        );
        vkGetDeviceQueue(
            m_device,
            m_queue_family_indices.PresentFamily.value(),
            0,
            &m_present_queue
        );
    }

    void Application::CreateSwapchain() {
        const SwapchainSupportInfo support =
            QuerySwapchainSupport(m_physical_device, m_surface);
        const VkSurfaceFormatKHR surface_format =
            SelectSwapchainSurfaceFormat(support.Formats);
        const VkPresentModeKHR present_mode =
            SelectSwapchainPresentMode(support.PresentModes);
        const VkExtent2D extent =
            SelectSwapchainExtent(support.Capabilities);

        const UInt32 min_image_count = support.Capabilities.minImageCount + 1;
        const bool min_image_count_supported
            =  support.Capabilities.maxImageCount == 0
            || min_image_count <= support.Capabilities.maxImageCount;
        const UInt32 image_count
            = min_image_count_supported
            ? min_image_count
            : support.Capabilities.maxImageCount;

        const UInt32 queue_family_indices[] {
            m_queue_family_indices.GraphicsFamily.value(),
            m_queue_family_indices.PresentFamily.value()
        };
        const bool single_family =
            queue_family_indices[0] == queue_family_indices[1];

        VkSharingMode sharing_mode;
        UInt32        queue_family_index_count;
        const UInt32* queue_family_indices_ptr;
        if (single_family) {
            sharing_mode             = VK_SHARING_MODE_EXCLUSIVE;
            queue_family_index_count = 0;
            queue_family_indices_ptr = nullptr;
        } else {
            sharing_mode             = VK_SHARING_MODE_CONCURRENT;
            queue_family_index_count = 2;
            queue_family_indices_ptr = queue_family_indices;
        }

        const VkSwapchainCreateInfoKHR create_info {
            VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            nullptr,
            0,
            m_surface,
            image_count,
            surface_format.format,
            surface_format.colorSpace,
            extent,
            1,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            sharing_mode,
            queue_family_index_count,
            queue_family_indices_ptr,
            support.Capabilities.currentTransform,
            VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            present_mode,
            VK_TRUE,
            VK_NULL_HANDLE
        };

        if (vkCreateSwapchainKHR(m_device, &create_info, nullptr, &m_swapchain)
                != VK_SUCCESS) {
            throw std::runtime_error("Failed to create swapchain.");
        }

        UInt32 n_images;
        vkGetSwapchainImagesKHR(m_device, m_swapchain, &n_images, nullptr);
        m_swapchain_images.resize(n_images);
        vkGetSwapchainImagesKHR(m_device, m_swapchain, &n_images,
            m_swapchain_images.data());

        m_swapchain_image_format = surface_format.format;
        m_swapchain_extent       = extent;
    }

    void Application::CreateSwapchainImageViews() {
        m_swapchain_image_views.resize(m_swapchain_images.size());
        for (size_t i = 0; i < m_swapchain_images.size(); i++) {
            static constexpr VkComponentMapping def_component_mapping{
                VK_COMPONENT_SWIZZLE_IDENTITY,
                VK_COMPONENT_SWIZZLE_IDENTITY,
                VK_COMPONENT_SWIZZLE_IDENTITY,
                VK_COMPONENT_SWIZZLE_IDENTITY
            };
            static constexpr VkImageSubresourceRange def_subresource_range {
                VK_IMAGE_ASPECT_COLOR_BIT,
                0,
                1,
                0,
                1
            };
            const VkImageViewCreateInfo create_info {
                VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                nullptr,
                0,
                m_swapchain_images[i],
                VK_IMAGE_VIEW_TYPE_2D,
                m_swapchain_image_format,
                def_component_mapping,
                def_subresource_range
            };
            if (vkCreateImageView(m_device, &create_info, nullptr,
                    &m_swapchain_image_views[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create image views.");
            }
        }
    }

    void Application::CreateRenderPass() {
        const VkAttachmentDescription color_attachment_desc {
            0,
            m_swapchain_image_format,
            VK_SAMPLE_COUNT_1_BIT,
            VK_ATTACHMENT_LOAD_OP_CLEAR,
            VK_ATTACHMENT_STORE_OP_STORE,
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            VK_ATTACHMENT_STORE_OP_DONT_CARE,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
        };
        const VkAttachmentReference color_attachment_ref {
            0,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        };
        const VkSubpassDescription subpass_desc {
            0,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            0,
            nullptr,
            1,
            &color_attachment_ref, // layout(location = 0) in frag shader
            nullptr,
            nullptr,
            0,
            nullptr
        };
        const VkSubpassDependency subpass_dependency {
            VK_SUBPASS_EXTERNAL,
            0,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            0,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            0
        };
        const VkRenderPassCreateInfo render_pass_info {
            VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            nullptr,
            0,
            1,
            &color_attachment_desc,
            1,
            &subpass_desc,
            1,
            &subpass_dependency
        };
        if (vkCreateRenderPass(m_device, &render_pass_info, nullptr,
                &m_render_pass) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create render pass!");
        }
    }

    void Application::CreateGraphicsPipeline() {
        const auto vertex_shader_bytecode =
            IO::ReadBinaryFile("res/shaders/vertex_shader.spv");
        const auto fragment_shader_bytecode =
            IO::ReadBinaryFile("res/shaders/fragment_shader.spv");
        const VkShaderModule
            vertex_shader_module =
                CreateShaderModule(vertex_shader_bytecode),
            fragment_shader_module =
                CreateShaderModule(fragment_shader_bytecode);

        const std::array<const VkPipelineShaderStageCreateInfo, 2>
                shader_stages {{
            {
                VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                nullptr,
                0,
                VK_SHADER_STAGE_VERTEX_BIT,
                vertex_shader_module,
                "main",
                nullptr
            },
            {
                VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                nullptr,
                0,
                VK_SHADER_STAGE_FRAGMENT_BIT,
                fragment_shader_module,
                "main",
                nullptr
            }
        }};

        const VkPipelineVertexInputStateCreateInfo vertex_input_info {
            VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            nullptr,
            0,
            0,
            nullptr,
            0,
            nullptr
        };

        const VkPipelineInputAssemblyStateCreateInfo input_assembly_info {
            VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            nullptr,
            0,
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            VK_FALSE
        };

        const VkViewport viewport {
            0.0f,
            0.0f,
            static_cast<float>(m_swapchain_extent.width),
            static_cast<float>(m_swapchain_extent.height),
            0.0f,
            1.0f
        };

        const VkRect2D scissor {
            { 0, 0 },
            m_swapchain_extent
        };

        const VkPipelineViewportStateCreateInfo viewport_state_info {
            VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            nullptr,
            0,
            1,
            &viewport,
            1,
            &scissor
        };

        const VkPipelineRasterizationStateCreateInfo rasterization_info {
            VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            nullptr,
            0,
            VK_FALSE,
            VK_FALSE,
            VK_POLYGON_MODE_FILL, // _LINE for wireframe?
            VK_CULL_MODE_BACK_BIT,
            VK_FRONT_FACE_CLOCKWISE,
            VK_FALSE,
            0.0f,
            0.0f,
            0.0f,
            1.0f
        };

        const VkPipelineMultisampleStateCreateInfo multisample_info {
            VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            nullptr,
            0,
            VK_SAMPLE_COUNT_1_BIT,
            VK_FALSE,
            1.0f,
            nullptr,
            VK_FALSE,
            VK_FALSE
        };

        // TODO: VkPipelineDepthStencilStateCreateInfo

        const VkPipelineColorBlendAttachmentState color_blend_attachment_state {
            VK_FALSE,
            VK_BLEND_FACTOR_ONE,
            VK_BLEND_FACTOR_ZERO,
            VK_BLEND_OP_ADD,
            VK_BLEND_FACTOR_ONE,
            VK_BLEND_FACTOR_ZERO,
            VK_BLEND_OP_ADD,
            VK_COLOR_COMPONENT_R_BIT
                | VK_COLOR_COMPONENT_G_BIT
                | VK_COLOR_COMPONENT_B_BIT
                | VK_COLOR_COMPONENT_A_BIT
        };

        const VkPipelineColorBlendStateCreateInfo color_blend_info {
            VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            nullptr,
            0,
            VK_FALSE,
            VK_LOGIC_OP_COPY,
            1,
            &color_blend_attachment_state,
            0.0f, 0.0f, 0.0f, 0.0f
        };

        /* // Dynamic State
        const std::array<const VkDynamicState, 2> dynamic_states {{
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_LINE_WIDTH
        }};
        const VkPipelineDynamicStateCreateInfo dynamic_state_info {
            VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            nullptr,
            0,
            dynamic_states.size(),
            dynamic_states.data()
        };
        */

        const VkPipelineLayoutCreateInfo pipeline_layout_info {
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            nullptr,
            0,
            0,
            nullptr,
            0,
            nullptr
        };

        if (vkCreatePipelineLayout(m_device, &pipeline_layout_info, nullptr,
                &m_pipeline_layout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create pipeline layout.");
        }

        const VkGraphicsPipelineCreateInfo pipeline_info {
            VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            nullptr,
            0,
            static_cast<UInt32>(shader_stages.size()),
            shader_stages.data(),
            &vertex_input_info,
            &input_assembly_info,
            nullptr,
            &viewport_state_info,
            &rasterization_info,
            &multisample_info,
            nullptr,
            &color_blend_info,
            nullptr, // dynamic state
            m_pipeline_layout,
            m_render_pass,
            0,
            VK_NULL_HANDLE,
            -1
        };

        if (vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1,
                &pipeline_info, nullptr, &m_graphics_pipeline) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create graphics pipeline.");
        }

        vkDestroyShaderModule(m_device, vertex_shader_module, nullptr);
        vkDestroyShaderModule(m_device, fragment_shader_module, nullptr);
    }

    void Application::CreateFramebuffers() {
        m_swapchain_framebuffers.resize(m_swapchain_image_views.size());
        for (size_t i = 0; i < m_swapchain_image_views.size(); i++) {
            const VkFramebufferCreateInfo framebuffer_info {
                VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                nullptr,
                0,
                m_render_pass,
                1,
                &m_swapchain_image_views[i],
                m_swapchain_extent.width,
                m_swapchain_extent.height,
                1
            };
            if (vkCreateFramebuffer(m_device, &framebuffer_info, nullptr,
                    &m_swapchain_framebuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create framebuffer.");
            }
        }
    }

    void Application::CreateCommandPool() {
        const VkCommandPoolCreateInfo cmd_pool_info {
            VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            nullptr,
            0,
            m_queue_family_indices.GraphicsFamily.value()
        };
        if (vkCreateCommandPool(m_device, &cmd_pool_info, nullptr,
                &m_cmd_pool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create command pool.");
        }
    }

    void Application::CreateCommandBuffers() {
        m_cmd_buffers.resize(m_swapchain_framebuffers.size());
        const VkCommandBufferAllocateInfo allocation_info {
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            nullptr,
            m_cmd_pool,
            VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            static_cast<UInt32>(m_cmd_buffers.size())
        };
        if (vkAllocateCommandBuffers(m_device, &allocation_info,
                m_cmd_buffers.data()) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate command buffers.");
        }

        for (size_t i = 0; i < m_cmd_buffers.size(); i++) {
            const VkCommandBufferBeginInfo begin_info {
                VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                nullptr,
                0,
                nullptr
            };
            if (vkBeginCommandBuffer(m_cmd_buffers[i], &begin_info)
                    != VK_SUCCESS) {
                throw std::runtime_error("Failed to begin recording command buffer.");
            }
            const VkClearValue clear_color = { 0.0f, 0.0f, 0.0f, 1.0f };
            const VkRenderPassBeginInfo render_pass_info {
                VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                nullptr,
                m_render_pass,
                m_swapchain_framebuffers[i],
                {{0, 0}, {m_swapchain_extent}},
                1,
                &clear_color
            };
            vkCmdBeginRenderPass(m_cmd_buffers[i], &render_pass_info,
                    VK_SUBPASS_CONTENTS_INLINE); {
                vkCmdBindPipeline(
                    m_cmd_buffers[i],
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    m_graphics_pipeline
                );
                vkCmdDraw(m_cmd_buffers[i], 3, 1, 0, 0);
            }
            vkCmdEndRenderPass(m_cmd_buffers[i]);
            if (vkEndCommandBuffer(m_cmd_buffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to record command buffer.");
            }
        }
    }

    void Application::CreateSynchronizationObjects() {
        m_sems_image_available.resize(MaxFramesInFlight);
        m_sems_render_finished.resize(MaxFramesInFlight);
        m_fens_in_flight.resize(MaxFramesInFlight);
        m_fens_images_in_flight.resize(m_swapchain_images.size(),
            VK_NULL_HANDLE);

        const VkSemaphoreCreateInfo semaphore_info {
            VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            nullptr,
            0
        };
        const VkFenceCreateInfo fence_info {
            VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            nullptr,
            VK_FENCE_CREATE_SIGNALED_BIT
        };
        for (size_t i = 0; i < MaxFramesInFlight; i++) {
            if (vkCreateSemaphore(m_device, &semaphore_info, nullptr,
                    &m_sems_image_available[i]) != VK_SUCCESS
                    || vkCreateSemaphore(m_device, &semaphore_info, nullptr,
                        &m_sems_render_finished[i]) != VK_SUCCESS
                    || vkCreateFence(m_device, &fence_info, nullptr,
                        &m_fens_in_flight[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create semaphores.");
            }
        }
    }

    void Application::RecreateSwapchain() {
        int width, height;
        glfwGetFramebufferSize(m_window, &width, &height);
        while (width == 0 || height == 0) {
            glfwWaitEvents();
            glfwGetFramebufferSize(m_window, &width, &height);
        }
        vkDeviceWaitIdle(m_device);

        CleanupSwapchain();

        CreateSwapchain();
        CreateSwapchainImageViews();
        CreateRenderPass();
        CreateGraphicsPipeline();
        CreateFramebuffers();
        CreateCommandBuffers();
    }

    void Application::CleanupSwapchain() {
        for (const auto& framebuffer : m_swapchain_framebuffers) {
            vkDestroyFramebuffer(m_device, framebuffer, nullptr);
        }
        vkFreeCommandBuffers(
            m_device,
            m_cmd_pool,
            static_cast<UInt32>(m_cmd_buffers.size()),
            m_cmd_buffers.data()
        );
        vkDestroyPipeline(m_device, m_graphics_pipeline, nullptr);
        vkDestroyPipelineLayout(m_device, m_pipeline_layout, nullptr);
        vkDestroyRenderPass(m_device, m_render_pass, nullptr);
        for (const auto& image_view : m_swapchain_image_views) {
            vkDestroyImageView(m_device, image_view, nullptr);
        }
        vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
    }

    bool Application::AreLayersSupported(
            const std::vector<const char*>& layers) const {
        UInt32 n_available_layers;
        vkEnumerateInstanceLayerProperties(&n_available_layers, nullptr);
        std::vector<VkLayerProperties> available_layers(n_available_layers);
        vkEnumerateInstanceLayerProperties(&n_available_layers,
            available_layers.data());

        for (const char* layer_name : layers) {
            const auto r = std::find_if(
                available_layers.begin(),
                available_layers.end(),
                [layer_name] (VkLayerProperties layer) {
                    return strcmp(layer_name, layer.layerName) == 0;
                }
            );
            if (r == available_layers.end())
                return false;
        }

        return true;
    }

    std::vector<const char*> Application::GetRequiredExtensions() const {
        UInt32 n_glfw_extensions;
        const char** glfw_extensions =
            glfwGetRequiredInstanceExtensions(&n_glfw_extensions);
        std::vector<const char*> required_extensions(glfw_extensions,
            glfw_extensions + n_glfw_extensions);
        KUMO_DEBUG_ONLY required_extensions.push_back(
            VK_EXT_DEBUG_UTILS_EXTENSION_NAME
        );
        return required_extensions;
    }

    bool Application::IsPhysicalDeviceSuitable(const VkPhysicalDevice& device,
            VkPhysicalDeviceProperties& properties,
            VkPhysicalDeviceFeatures& features) const {
        vkGetPhysicalDeviceProperties(device, &properties);
        vkGetPhysicalDeviceFeatures(device, &features);
        const QueueFamilyIndices queue_family_indices =
            FindQueueFamilies(device);
        const bool extensions_supported =
            CheckDeviceExtensionSupport(device, DeviceExtensions);
        bool swapchain_is_adequate = false;
        if (extensions_supported) {
            const SwapchainSupportInfo swapchain_support_info =
                QuerySwapchainSupport(device, m_surface);
            swapchain_is_adequate
                =  !swapchain_support_info.Formats.empty()
                && !swapchain_support_info.PresentModes.empty();
        }

        return queue_family_indices.IsComplete()
            && extensions_supported
            && swapchain_is_adequate;
    }

    QueueFamilyIndices Application::FindQueueFamilies(
            const VkPhysicalDevice& device) const {
        QueueFamilyIndices indices;
        UInt32 n_queue_families;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &n_queue_families,
            nullptr);
        std::vector<VkQueueFamilyProperties> queue_families(n_queue_families);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &n_queue_families,
            queue_families.data());
        for (UInt32 i = 0; i < n_queue_families; i++) {
            const auto& queue_family = queue_families[i];
            if (queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT)
                indices.GraphicsFamily = i;
            VkBool32 present_support;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface,
                &present_support);
            if (present_support)
                indices.PresentFamily = i;
            if (indices.IsComplete())
                break;
        }
        return indices;
    }

    bool Application::CheckDeviceExtensionSupport(
            const VkPhysicalDevice& device,
            const std::vector<const char*>& extensions) const {
        UInt32 n_available_extensions;
        vkEnumerateDeviceExtensionProperties(device, nullptr,
            &n_available_extensions, nullptr);
        std::vector<VkExtensionProperties> available_extensions(
            n_available_extensions);
        vkEnumerateDeviceExtensionProperties(device, nullptr,
            &n_available_extensions, available_extensions.data());
        std::set<std::string> unavailable_extensions(extensions.begin(),
            extensions.end());
        for (const auto& extension : available_extensions) {
            unavailable_extensions.erase(extension.extensionName);
        }
        return unavailable_extensions.empty();
    }

    SwapchainSupportInfo Application::QuerySwapchainSupport(
            const VkPhysicalDevice& device, const VkSurfaceKHR& surface) const {
        SwapchainSupportInfo info;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface,
            &info.Capabilities);
        UInt32 n_formats;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &n_formats,
            nullptr);
        if (n_formats != 0) {
            info.Formats.resize(n_formats);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &n_formats,
                info.Formats.data());
        }
        UInt32 n_present_modes;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface,
            &n_present_modes, info.PresentModes.data());
        if (n_present_modes != 0) {
            info.PresentModes.resize(n_present_modes);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface,
                &n_present_modes, info.PresentModes.data());
        }
        return info;
    }

    VkSurfaceFormatKHR Application::SelectSwapchainSurfaceFormat(
            const std::vector<VkSurfaceFormatKHR>& available_formats) const {
        for (const auto& format : available_formats) {
            if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
                    format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return format;
            }
        }
        std::cout << "Can't find ideal surface format; going with default."
            << std::endl;
        return available_formats[0];
    }

    VkPresentModeKHR Application::SelectSwapchainPresentMode(
            const std::vector<VkPresentModeKHR>& available_modes) const {
        for (const auto& mode : available_modes) {
            if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
                return mode;
        }
        // FIFO should always be available.
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D Application::SelectSwapchainExtent(
            const VkSurfaceCapabilitiesKHR& capabilities) const {
        static constexpr UInt32 SpecVal = std::numeric_limits<UInt32>::max();
        if (capabilities.currentExtent.width != SpecVal)
            return capabilities.currentExtent;
        
        int width, height;
        glfwGetFramebufferSize(m_window, &width, &height);

        return {
            std::clamp(
                static_cast<UInt32>(width),
                capabilities.minImageExtent.width,
                capabilities.maxImageExtent.width
            ),
            std::clamp(
                static_cast<UInt32>(height),
                capabilities.minImageExtent.height,
                capabilities.maxImageExtent.height
            )
        };
    }

    VkShaderModule Application::CreateShaderModule(
        const std::vector<Byte>& bytecode) const {
        const VkShaderModuleCreateInfo create_info{
            VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            nullptr,
            0,
            bytecode.size(),
            reinterpret_cast<const UInt32*>(bytecode.data())
        };
        VkShaderModule shader_module;
        if (vkCreateShaderModule(m_device, &create_info, nullptr,
                &shader_module) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create shader module.");
        }
        return shader_module;
    }

    void Application::SetupDebugMessenger() {
        auto vkCreateDebugUtilsMessengerEXT =
            reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
                vkGetInstanceProcAddr(
                    m_instance,
                    "vkCreateDebugUtilsMessengerEXT"
                )
            );
        if (!vkCreateDebugUtilsMessengerEXT) {
            throw std::runtime_error(
                "Function vkCreateDebugUtilsMessengerEXT failed to load."
            );
        }
        if (vkCreateDebugUtilsMessengerEXT(m_instance,
                &m_debug_messenger_create_info, nullptr,
                &m_debug_messenger) != VK_SUCCESS) {
            throw std::runtime_error("Failed to setup debug messenger.");
        }
    }

    static std::string VulkanDebugMessageTypeName(
            VkDebugUtilsMessageTypeFlagsEXT type) {
        switch (type) {
        case VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT:
            return "GENERAL";
        case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT:
            return "VALIDATION";
        case VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT:
            return "PERFORMANCE";
        default:
            return "UNKNOWN";
        }
    }

    VKAPI_ATTR VkBool32 VKAPI_CALL Application::VulkanDebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT      severity,
        VkDebugUtilsMessageTypeFlagsEXT             type,
        const VkDebugUtilsMessengerCallbackDataEXT* data,
        void*
    ) {
        if (severity >= VulkanLogSeverity) {
            std::cerr << "[VULKAN] " << VulkanDebugMessageTypeName(type)
                << ": " << data->pMessage << std::endl;
        }
        return VK_FALSE;
    }

    void Application::GLFWFramebufferResizeCallback(
        GLFWwindow* window,
        int width,
        int height
    ) {
        Application* app = reinterpret_cast<Application*>(
            glfwGetWindowUserPointer(window)
        );
        app->m_framebuffer_resized = true;
    }

}