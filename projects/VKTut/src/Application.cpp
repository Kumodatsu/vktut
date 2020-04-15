#include "Common.hpp"
#include "Application.hpp"
#include "IO.hpp"
#include "Vertex.hpp"

#include "STB/stb_image.h"
#include "tinyobj/tiny_obj_loader.h"

#include <glm/gtc/matrix_transform.hpp>

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
        m_light = {
            { 100.0f, 100.0f, 100.0f },
            { 100.0f, 100.0f, 100.0f }
        };
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
        CreateDescriptorSetLayout();
        CreateGraphicsPipeline();
        CreateCommandPool();
        CreateDepthResources();
        CreateFramebuffers();
        CreateTextureImage("res/textures/chalet.jpg");
        CreateTextureImageView();
        CreateTextureSampler();
        LoadModel("res/models/chalet.obj");
        CreateVertexBuffer();
        CreateIndexBuffer();
        CreateUniformBuffers();
        CreateDescriptorPool();
        CreateDescriptorSets();
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
        vkDestroySampler(m_device, m_texture_sampler, nullptr);
        vkDestroyImageView(m_device, m_texture_image_view, nullptr);
        vkDestroyImage(m_device, m_texture_image, nullptr);
        vkFreeMemory(m_device, m_mem_texture_image, nullptr);
        vkDestroyDescriptorSetLayout(m_device, m_descriptor_set_layout,
            nullptr);
        vkDestroyBuffer(m_device, m_index_buffer, nullptr);
        vkFreeMemory(m_device, m_mem_index_buffer, nullptr);
        vkDestroyBuffer(m_device, m_vertex_buffer, nullptr);
        vkFreeMemory(m_device, m_mem_vertex_buffer, nullptr);
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

        UpdateUniformBuffer(image_index);

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

    void Application::UpdateUniformBuffer(UInt32 current_image) {
        static auto start_time = std::chrono::high_resolution_clock::now();
        auto current_time = std::chrono::high_resolution_clock::now();
        const float dt =
            std::chrono::duration<float, std::chrono::seconds::period>(
                current_time - start_time
            ).count();

        UniformBufferObject ubo;
        ubo.Model = glm::rotate(
            glm::mat4(1.0f),
            dt * glm::radians(90.0f),
            glm::vec3(0.0f, 0.0f, 1.0f)
        );
        ubo.View = glm::lookAt(
            glm::vec3(2.0f, 2.0f, 2.0f),
            glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 0.0f, 1.0f)
        );
        ubo.Projection = glm::perspective(
            glm::radians(45.0f),
            m_swapchain_extent.width
                / static_cast<float>(m_swapchain_extent.height),
            0.1f,
            10.0f
        );
        ubo.Projection[1][1] *= -1.0f;
        void* data;
        vkMapMemory(m_device, m_mems_uniform_buffers[current_image], 0,
            sizeof(UniformBufferObject), 0, &data);
        memcpy(data, &ubo, sizeof(UniformBufferObject));
        vkUnmapMemory(m_device, m_mems_uniform_buffers[current_image]);
    }

    void Application::LoadModel(const std::string& path) {
        tinyobj::attrib_t                attributes;
        std::vector<tinyobj::shape_t>    shapes;
        std::vector<tinyobj::material_t> materials;
        std::string warning, error;
        if (!tinyobj::LoadObj(&attributes, &shapes, &materials, &warning,
                &error, IO::VFS::GetPath(path).c_str())) {
            throw std::runtime_error(warning + error);
        }
        m_mesh.Vertices.clear();
        m_mesh.Indices.clear();
        std::unordered_map<Vertex, Mesh::Index> unique_vertices {};
        for (const auto& shape : shapes) {
            for (const auto& index : shape.mesh.indices) {
                const Vertex vertex {
                    {
                        attributes.vertices[3 * index.vertex_index + 0],
                        attributes.vertices[3 * index.vertex_index + 1],
                        attributes.vertices[3 * index.vertex_index + 2]
                    },
                    { 1.0f, 1.0f, 1.0f },
                    {
                        attributes.texcoords[2 * index.texcoord_index + 0],
                        1.0f - attributes.texcoords[2 * index.texcoord_index + 1]
                    }
                };
                if (unique_vertices.count(vertex) == 0) {
                    unique_vertices[vertex] =
                        static_cast<Mesh::Index>(m_mesh.Vertices.size());
                    m_mesh.Vertices.push_back(vertex);
                }
                m_mesh.Indices.push_back(unique_vertices[vertex]);
            }
        }
        std::reverse(m_mesh.Indices.begin(), m_mesh.Indices.end());
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
        VkPhysicalDeviceFeatures features { };
        features.samplerAnisotropy = VK_TRUE;
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
            m_swapchain_image_views[i] = CreateImageView(
                m_swapchain_images[i],
                m_swapchain_image_format,
                VK_IMAGE_ASPECT_COLOR_BIT
            );
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
        const VkAttachmentDescription depth_attachment_desc {
            0,
            FindDepthFormat(),
            VK_SAMPLE_COUNT_1_BIT,
            VK_ATTACHMENT_LOAD_OP_CLEAR,
            VK_ATTACHMENT_STORE_OP_DONT_CARE,
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            VK_ATTACHMENT_STORE_OP_DONT_CARE,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        };
        const VkAttachmentReference color_attachment_ref {
            0,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        };
        const VkAttachmentReference depth_attachment_ref {
            1,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        };
        const VkSubpassDescription subpass_desc {
            0,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            0,
            nullptr,
            1,
            &color_attachment_ref, // layout(location = 0) in frag shader
            nullptr,
            &depth_attachment_ref,
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
        const std::array<VkAttachmentDescription, 2> attachments {{
            color_attachment_desc,
            depth_attachment_desc
        }};
        const VkRenderPassCreateInfo render_pass_info {
            VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            nullptr,
            0,
            static_cast<UInt32>(attachments.size()),
            attachments.data(),
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

    void Application::CreateDescriptorSetLayout() {
        const std::array<VkDescriptorSetLayoutBinding, 2> ubo_layout_bindings {{
            {
                0,
                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                1,
                VK_SHADER_STAGE_VERTEX_BIT,
                nullptr
            },
            {
                1,
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                1,
                VK_SHADER_STAGE_FRAGMENT_BIT,
                nullptr
            }
        }};
        const VkDescriptorSetLayoutCreateInfo layout_info {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            nullptr,
            0,
            static_cast<UInt32>(ubo_layout_bindings.size()),
            ubo_layout_bindings.data()
        };
        if (vkCreateDescriptorSetLayout(m_device, &layout_info, nullptr,
                &m_descriptor_set_layout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create descriptor set layout.");
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

        const auto binding_description    = Vertex::GetBindingDescription();
        const auto attribute_descriptions = Vertex::GetAttributeDescriptions();
        const VkPipelineVertexInputStateCreateInfo vertex_input_info {
            VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            nullptr,
            0,
            1,
            &binding_description,
            static_cast<UInt32>(attribute_descriptions.size()),
            attribute_descriptions.data()
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
            1,
            &m_descriptor_set_layout,
            0,
            nullptr
        };

        if (vkCreatePipelineLayout(m_device, &pipeline_layout_info, nullptr,
                &m_pipeline_layout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create pipeline layout.");
        }

        const VkPipelineDepthStencilStateCreateInfo depth_stencil_info {
            VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            nullptr,
            0,
            VK_TRUE,
            VK_TRUE,
            VK_COMPARE_OP_LESS,
            VK_FALSE,
            VK_FALSE,
            {},
            {},
            0.0f,
            1.0f
        };

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
            &depth_stencil_info,
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
            const std::array<VkImageView, 2> attachments {{
                m_swapchain_image_views[i],
                m_depth_image_view
            }};
            const VkFramebufferCreateInfo framebuffer_info {
                VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                nullptr,
                0,
                m_render_pass,
                static_cast<UInt32>(attachments.size()),
                attachments.data(),
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

    void Application::CreateDepthResources() {
        const VkFormat depth_format = FindDepthFormat();
        CreateImage(
            m_swapchain_extent.width,
            m_swapchain_extent.height,
            depth_format,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            m_depth_image,
            m_mem_depth_image
        );
        m_depth_image_view = CreateImageView(
            m_depth_image,
            depth_format,
            VK_IMAGE_ASPECT_DEPTH_BIT
        );
        TransitionImageLayout(
            m_depth_image,
            depth_format,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        );
    }

    void Application::CreateTextureImage(const std::string& path) {
        const char* used_path = IO::VFS::GetPath(path).c_str();
        if (!std::filesystem::exists(used_path)) {
            std::cout << "Warning: texture " << path << " doesn't exist. "
                << "Using default texture." << std::endl;
            used_path = IO::VFS::GetPath("res/textures/missingno.png").c_str();
        }

        int width, height, n_channels;
        stbi_uc* pixels = stbi_load(used_path, &width, &height, &n_channels,
            STBI_rgb_alpha);
        if (!pixels) {
            throw std::runtime_error("Failed to load texture image.");
        }
        const VkDeviceSize size = width * height * 4;

        VkBuffer staging_buffer;
        VkDeviceMemory mem_staging_buffer;
        CreateBuffer(
            size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            staging_buffer,
            mem_staging_buffer
        );
        void* data;
        vkMapMemory(m_device, mem_staging_buffer, 0, size, 0, &data);
        memcpy(data, pixels, static_cast<USize>(size));
        vkUnmapMemory(m_device, mem_staging_buffer);
        stbi_image_free(pixels);

        CreateImage(
            static_cast<UInt32>(width),
            static_cast<UInt32>(height),
            VK_FORMAT_R8G8B8A8_SRGB,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            m_texture_image,
            m_mem_texture_image
        );

        TransitionImageLayout(
            m_texture_image,
            VK_FORMAT_R8G8B8A8_SRGB,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
        );
        CopyBufferToImage(
            staging_buffer,
            m_texture_image,
            static_cast<UInt32>(width),
            static_cast<UInt32>(height)
        );
        TransitionImageLayout(
            m_texture_image,
            VK_FORMAT_R8G8B8A8_SRGB,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        );

        vkDestroyBuffer(m_device, staging_buffer, nullptr);
        vkFreeMemory(m_device, mem_staging_buffer, nullptr);
    }

    void Application::CreateTextureImageView() {
        m_texture_image_view = CreateImageView(
            m_texture_image,
            VK_FORMAT_R8G8B8A8_SRGB,
            VK_IMAGE_ASPECT_COLOR_BIT
        );
    }

    void Application::CreateImage(UInt32 width, UInt32 height, VkFormat format,
            VkImageTiling tiling, VkImageUsageFlags usage,
            VkMemoryPropertyFlags properties, VkImage& out_image,
            VkDeviceMemory& out_memory) const {
        const VkImageCreateInfo image_info {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            nullptr,
            0,
            VK_IMAGE_TYPE_2D,
            format,
            {static_cast<UInt32>(width), static_cast<UInt32>(height), 1},
            1,
            1,
            VK_SAMPLE_COUNT_1_BIT,
            tiling,
            usage,
            VK_SHARING_MODE_EXCLUSIVE,
            0,
            nullptr,
            VK_IMAGE_LAYOUT_UNDEFINED
        };
        if (vkCreateImage(m_device, &image_info, nullptr, &out_image)
                != VK_SUCCESS) {
            throw std::runtime_error("Failed to create image.");
        }
        VkMemoryRequirements memory_requirements;
        vkGetImageMemoryRequirements(m_device, out_image,
            &memory_requirements);
        const VkMemoryAllocateInfo allocation_info {
            VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            nullptr,
            memory_requirements.size,
            SelectMemoryType(memory_requirements.memoryTypeBits,
                properties)
        };
        if (vkAllocateMemory(m_device, &allocation_info, nullptr,
                &out_memory) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate image memory.");
        }
        vkBindImageMemory(m_device, out_image, out_memory, 0);
    }

    void Application::CreateVertexBuffer() {
        const VkDeviceSize buffer_size =
            sizeof(Vertex) * m_mesh.Vertices.size();

        VkBuffer       staging_buffer;
        VkDeviceMemory mem_staging_buffer;
        CreateBuffer(
            buffer_size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            staging_buffer,
            mem_staging_buffer
        );

        void* data;
        vkMapMemory(m_device, mem_staging_buffer, 0, buffer_size, 0,
            &data);
        memcpy(data, m_mesh.Vertices.data(), static_cast<USize>(buffer_size));
        vkUnmapMemory(m_device, mem_staging_buffer);

        CreateBuffer(
            buffer_size,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
                | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            m_vertex_buffer,
            m_mem_vertex_buffer
        );
        CopyBuffer(staging_buffer, m_vertex_buffer, buffer_size);
        
        vkDestroyBuffer(m_device, staging_buffer, nullptr);
        vkFreeMemory(m_device, mem_staging_buffer, nullptr);
    }

    void Application::CreateIndexBuffer() {
        const VkDeviceSize buffer_size =
            sizeof(Mesh::Index) * m_mesh.Indices.size();

        VkBuffer       staging_buffer;
        VkDeviceMemory mem_staging_buffer;
        CreateBuffer(
            buffer_size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            staging_buffer,
            mem_staging_buffer
        );

        void* data;
        vkMapMemory(m_device, mem_staging_buffer, 0, buffer_size, 0,
            &data);
        memcpy(data, m_mesh.Indices.data(), static_cast<USize>(buffer_size));
        vkUnmapMemory(m_device, mem_staging_buffer);

        CreateBuffer(
            buffer_size,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT
                | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            m_index_buffer,
            m_mem_index_buffer
        );
        CopyBuffer(staging_buffer, m_index_buffer, buffer_size);

        vkDestroyBuffer(m_device, staging_buffer, nullptr);
        vkFreeMemory(m_device, mem_staging_buffer, nullptr);
    }

    void Application::CreateUniformBuffers() {
        const VkDeviceSize buffer_size = sizeof(UniformBufferObject);
        m_uniform_buffers.resize(m_swapchain_images.size());
        m_mems_uniform_buffers.resize(m_swapchain_images.size());
        for (USize i = 0; i < m_swapchain_images.size(); i++) {
            CreateBuffer(
                buffer_size,
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                    | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                m_uniform_buffers[i],
                m_mems_uniform_buffers[i]
            );
        }
    }

    void Application::CreateDescriptorPool() {
        const std::array<VkDescriptorPoolSize, 2> pool_sizes {{
            {
                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                static_cast<UInt32>(m_swapchain_images.size())
            },
            {
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                static_cast<UInt32>(m_swapchain_images.size())
            }
        }};
        const VkDescriptorPoolCreateInfo pool_info {
            VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            nullptr,
            0,
            static_cast<UInt32>(m_swapchain_images.size()),
            static_cast<UInt32>(pool_sizes.size()),
            pool_sizes.data()
        };
        if (vkCreateDescriptorPool(m_device, &pool_info, nullptr,
                &m_descriptor_pool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create descriptor pool.");
        }
    }

    void Application::CreateDescriptorSets() {
        std::vector<VkDescriptorSetLayout> layouts(
            m_swapchain_images.size(),
            m_descriptor_set_layout
        );
        const VkDescriptorSetAllocateInfo allocation_info {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            nullptr,
            m_descriptor_pool,
            static_cast<UInt32>(m_swapchain_images.size()),
            layouts.data()
        };
        m_descriptor_sets.resize(m_swapchain_images.size());
        if (vkAllocateDescriptorSets(m_device, &allocation_info,
                m_descriptor_sets.data()) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate descriptor sets.");
        }
        for (USize i = 0; i < m_swapchain_images.size(); i++) {
            const VkDescriptorBufferInfo buffer_info {
                m_uniform_buffers[i],
                0,
                sizeof(UniformBufferObject)
            };
            const VkDescriptorImageInfo image_info {
                m_texture_sampler,
                m_texture_image_view,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            };
            const std::array<VkWriteDescriptorSet, 2> descriptor_set_writes {{
                {
                    VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    nullptr,
                    m_descriptor_sets[i],
                    0,
                    0,
                    1,
                    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    nullptr,
                    &buffer_info,
                    nullptr
                },
                {
                    VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    nullptr,
                    m_descriptor_sets[i],
                    1,
                    0,
                    1,
                    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    &image_info,
                    nullptr,
                    nullptr
                }
            }};
            vkUpdateDescriptorSets(
                m_device,
                static_cast<UInt32>(descriptor_set_writes.size()),
                descriptor_set_writes.data(),
                0,
                nullptr
            );
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
            const VkCommandBuffer& buffer = m_cmd_buffers[i];
            const VkCommandBufferBeginInfo begin_info {
                VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                nullptr,
                0,
                nullptr
            };
            if (vkBeginCommandBuffer(buffer, &begin_info) != VK_SUCCESS) {
                throw std::runtime_error(
                    "Failed to begin recording command buffer."
                );
            }
            // /!\ Caution: weird union stuff going on
            // Order of clear values must be same as order of attachments
            const std::array<VkClearValue, 2> clear_values {{
                { 0.0f, 0.0f, 0.0f, 1.0f },
                { 1.0f, 0U }
            }};
            const VkRenderPassBeginInfo render_pass_info {
                VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                nullptr,
                m_render_pass,
                m_swapchain_framebuffers[i],
                {{0, 0}, {m_swapchain_extent}},
                static_cast<UInt32>(clear_values.size()),
                clear_values.data()
            };
            vkCmdBeginRenderPass(buffer, &render_pass_info,
                    VK_SUBPASS_CONTENTS_INLINE); {
                vkCmdBindPipeline(
                    buffer,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    m_graphics_pipeline
                );
                VkDeviceSize offset = 0;
                vkCmdBindVertexBuffers(buffer, 0, 1,  &m_vertex_buffer,
                    &offset);
                vkCmdBindIndexBuffer(buffer, m_index_buffer, 0,
                    Mesh::IndexType);
                vkCmdBindDescriptorSets(
                    buffer,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    m_pipeline_layout,
                    0,
                    1,
                    &m_descriptor_sets[i],
                    0,
                    nullptr
                );
                vkCmdDrawIndexed(
                    buffer,
                    static_cast<Mesh::Index>(m_mesh.Indices.size()),
                    1,
                    0,
                    0,
                    0
                );
            }
            vkCmdEndRenderPass(buffer);
            if (vkEndCommandBuffer(buffer) != VK_SUCCESS) {
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
        CreateDepthResources();
        CreateFramebuffers();
        CreateUniformBuffers();
        CreateDescriptorPool();
        CreateDescriptorSets();
        CreateCommandBuffers();
    }

    void Application::CleanupSwapchain() {
        vkDestroyImageView(m_device, m_depth_image_view, nullptr);
        vkDestroyImage(m_device, m_depth_image, nullptr);
        vkFreeMemory(m_device, m_mem_depth_image, nullptr);
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
        for (USize i = 0; i < m_swapchain_images.size(); i++) {
            vkDestroyBuffer(m_device, m_uniform_buffers[i], nullptr);
            vkFreeMemory(m_device, m_mems_uniform_buffers[i], nullptr);
        }
        vkDestroyDescriptorPool(m_device, m_descriptor_pool, nullptr);
    }

    void Application::CreateTextureSampler() {
        const VkSamplerCreateInfo sampler_info {
            VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            nullptr,
            0,
            VK_FILTER_LINEAR,
            VK_FILTER_LINEAR,
            VK_SAMPLER_MIPMAP_MODE_LINEAR,
            VK_SAMPLER_ADDRESS_MODE_REPEAT,
            VK_SAMPLER_ADDRESS_MODE_REPEAT,
            VK_SAMPLER_ADDRESS_MODE_REPEAT,
            0.0f,
            VK_TRUE,
            16.0f,
            VK_FALSE,
            VK_COMPARE_OP_ALWAYS,
            0.0f,
            0.0f,
            VK_BORDER_COLOR_INT_OPAQUE_BLACK,
            VK_FALSE
        };
        if (vkCreateSampler(m_device, &sampler_info, nullptr,
                &m_texture_sampler) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create texture sampler.");
        }
    }

    VkFormat Application::FindSupportedFormat(
        const std::vector<VkFormat>& candidates,
        VkImageTiling tiling,
        VkFormatFeatureFlags features
    ) const {
        for (const VkFormat& format : candidates) {
            VkFormatProperties properties;
            vkGetPhysicalDeviceFormatProperties(m_physical_device, format,
                &properties);
            const VkFormatFeatureFlags flags
                = tiling == VK_IMAGE_TILING_LINEAR  ? properties.linearTilingFeatures
                : tiling == VK_IMAGE_TILING_OPTIMAL ? properties.optimalTilingFeatures
                : throw std::invalid_argument("Invalid tiling.");
            if ((flags & features) == features)
                return format;
        }
        throw std::runtime_error("Failed to find supported format.");
    }

    VkFormat Application::FindDepthFormat() const {
        return FindSupportedFormat(
            {
                VK_FORMAT_D32_SFLOAT,
                VK_FORMAT_D32_SFLOAT_S8_UINT,
                VK_FORMAT_D24_UNORM_S8_UINT
            },
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
        );
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
        bool feature_complete = features.samplerAnisotropy;

        return queue_family_indices.IsComplete()
            && extensions_supported
            && swapchain_is_adequate
            && feature_complete;
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

    UInt32 Application::SelectMemoryType(UInt32 type_filter,
            VkMemoryPropertyFlags properties) const {
        VkPhysicalDeviceMemoryProperties mem_properties;
        vkGetPhysicalDeviceMemoryProperties(m_physical_device,
            &mem_properties);
        for (UInt32 i = 0; i < mem_properties.memoryTypeCount; i++) {
            const VkMemoryPropertyFlags property_flags =
                mem_properties.memoryTypes[i].propertyFlags;
            const bool
                adheres_filter = type_filter & (1 << i),
                has_properties = (property_flags & properties) == properties;
            if (adheres_filter && has_properties) {
                return i;
            }
        }
        throw std::runtime_error("Failed to find suitable memory type.");
    }

    VkImageView Application::CreateImageView(VkImage image, VkFormat format,
            VkImageAspectFlags aspects) const {
        static constexpr VkComponentMapping def_component_mapping{
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY
        };
        const VkImageSubresourceRange def_subresource_range {
            aspects,
            0,
            1,
            0,
            1
        };
        const VkImageViewCreateInfo view_info {
            VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            nullptr,
            0,
            image,
            VK_IMAGE_VIEW_TYPE_2D,
            format,
            def_component_mapping,
            def_subresource_range
        };
        VkImageView image_view;
        if (vkCreateImageView(m_device, &view_info, nullptr,
                &image_view) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create texture image view.");
        }
        return image_view;
    }

    void Application::TransitionImageLayout(VkImage image, VkFormat format,
            VkImageLayout old_layout, VkImageLayout new_layout) const {
        VkAccessFlags src_access_mask, dst_access_mask;
        VkPipelineStageFlags src_stage, dst_stage;
        VkImageAspectFlags aspects = 0;

        if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED
                && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
            src_access_mask = 0;
            dst_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT;
            src_stage       = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            dst_stage       = VK_PIPELINE_STAGE_TRANSFER_BIT;
        } else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
                && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            src_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT;
            dst_access_mask = VK_ACCESS_SHADER_READ_BIT;
            src_stage       = VK_PIPELINE_STAGE_TRANSFER_BIT;
            dst_stage       = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        } else if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED
                && new_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
            src_access_mask = 0;
            dst_access_mask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT
                | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            dst_stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        } else {
            throw std::invalid_argument("Unsupported layout transition.");
        }

        if (new_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
            aspects |= VK_IMAGE_ASPECT_DEPTH_BIT;
            if (HasStencilComponent(format))
                aspects |= VK_IMAGE_ASPECT_STENCIL_BIT;
        } else {
            aspects |= VK_IMAGE_ASPECT_COLOR_BIT;
        }

        const VkImageMemoryBarrier barrier {
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            nullptr,
            src_access_mask,
            dst_access_mask,
            old_layout,
            new_layout,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            image,
            {
                aspects,
                0,
                1,
                0,
                1
            }
        };
        
        const VkCommandBuffer cmd_buffer = BeginSingleTimeCommands();

        vkCmdPipelineBarrier(
            cmd_buffer,
            src_stage,
            dst_stage,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &barrier
        );

        EndSingleTimeCommands(cmd_buffer);
    }

    void Application::CopyBufferToImage(const VkBuffer& buffer,
            const VkImage& image, UInt32 width, UInt32 height) const {
        const VkBufferImageCopy region {
            0,
            0,
            0,
            {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
            {0, 0, 0},
            {width, height, 1}
        };
        
        const VkCommandBuffer cmd_buffer = BeginSingleTimeCommands();

        vkCmdCopyBufferToImage(
            cmd_buffer,
            buffer,
            image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &region
        );

        EndSingleTimeCommands(cmd_buffer);
    }

    void Application::CreateBuffer(
        VkDeviceSize size,
        VkBufferUsageFlags usage_flags,
        VkMemoryPropertyFlags property_flags,
        VkBuffer& out_buffer,
        VkDeviceMemory& out_memory
    ) const {
        const VkBufferCreateInfo buffer_info {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            nullptr,
            0,
            size,
            usage_flags,
            VK_SHARING_MODE_EXCLUSIVE,
            0,
            nullptr
        };
        if (vkCreateBuffer(m_device, &buffer_info, nullptr, &out_buffer)
                != VK_SUCCESS) {
            throw std::runtime_error("Failed to create vertex buffer.");
        }
        VkMemoryRequirements memory_requirements;
        vkGetBufferMemoryRequirements(m_device, out_buffer,
            &memory_requirements);
        const VkMemoryAllocateInfo allocation_info {
            VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            nullptr,
            memory_requirements.size,
            SelectMemoryType(
                memory_requirements.memoryTypeBits,
                property_flags
            )
        };
        if (vkAllocateMemory(m_device, &allocation_info, nullptr,
                &out_memory) != VK_SUCCESS) {
            throw std::runtime_error(
                "Failed to allocate vertex buffer memory."
            );
        }
        vkBindBufferMemory(m_device, out_buffer, out_memory, 0);
    }

    void Application::CopyBuffer(const VkBuffer& src, const VkBuffer& dst,
            VkDeviceSize size) const {
        const VkCommandBuffer cmd_buffer = BeginSingleTimeCommands();
        
        const VkBufferCopy copy_region { 0, 0, size };
        vkCmdCopyBuffer(cmd_buffer, src, dst, 1, &copy_region);
        
        EndSingleTimeCommands(cmd_buffer);        
    }

    VkCommandBuffer Application::BeginSingleTimeCommands() const {
        const VkCommandBufferAllocateInfo allocation_info {
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            nullptr,
            m_cmd_pool,
            VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            1
        };
        VkCommandBuffer cmd_buffer;
        vkAllocateCommandBuffers(m_device, &allocation_info, &cmd_buffer);
        const VkCommandBufferBeginInfo begin_info {
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            nullptr,
            VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            nullptr
        };
        vkBeginCommandBuffer(cmd_buffer, &begin_info);
        return cmd_buffer;
    }

    void Application::EndSingleTimeCommands(const VkCommandBuffer& cmd_buffer)
            const {
        vkEndCommandBuffer(cmd_buffer);
        const VkSubmitInfo submit_info {
            VK_STRUCTURE_TYPE_SUBMIT_INFO,
            nullptr,
            0,
            nullptr,
            nullptr,
            1,
            &cmd_buffer,
            0,
            nullptr
        };
        vkQueueSubmit(m_graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
        vkQueueWaitIdle(m_graphics_queue);
        vkFreeCommandBuffers(m_device, m_cmd_pool, 1, &cmd_buffer);
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
        int,
        int
    ) {
        Application* app = reinterpret_cast<Application*>(
            glfwGetWindowUserPointer(window)
        );
        app->m_framebuffer_resized = true;
    }

}