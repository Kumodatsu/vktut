#include "Common.hpp"
#include "Application.hpp"

namespace Kumo {

    static const std::vector<const char*> ValidationLayers {
        "VK_LAYER_KHRONOS_validation"
    };

    static const std::vector<const char*> DeviceExtensions {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    Application::Application()
        : m_window(nullptr)
        , m_instance()
        , m_debug_messenger()
        , m_debug_messenger_create_info()
        , m_physical_device(VK_NULL_HANDLE)
        , m_device()
        , m_surface()
        , m_queue_family_indices()
        , m_graphics_queue()
        , m_present_queue()
        , m_swapchain()
        , m_swapchain_images()
    { }

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
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        m_window = glfwCreateWindow(
            static_cast<int>(WindowWidth),
            static_cast<int>(WindowHeight),
            "Vulkan",
            nullptr,
            nullptr
        );

        if (!m_window)
            throw std::runtime_error("Failed to create window.");
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
    }

    void Application::RunLoop() {
        while (!glfwWindowShouldClose(m_window)) {
            glfwPollEvents();
        }
    }

    void Application::Cleanup() {
        vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
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

        const VkInstanceCreateInfo create_info {
            VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            nullptr,
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
        for (const auto& device : devices) {
            VkPhysicalDeviceProperties properties;
            VkPhysicalDeviceFeatures   features;
            if (IsPhysicalDeviceSuitable(device, properties, features)) {
                m_physical_device = device;
                std::cout << "Using device:" << std::endl
                    << "\t" << properties.deviceName << std::endl;
                return;
            }
        }
        
        throw std::runtime_error("Found no suitable GPU.");
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
        return {
            std::max(
                capabilities.minImageExtent.width,
                std::min(capabilities.maxImageExtent.width, WindowWidth)
            ),
            std::max(
                capabilities.minImageExtent.height,
                std::min(capabilities.maxImageExtent.height, WindowHeight)
            )
        };
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

}