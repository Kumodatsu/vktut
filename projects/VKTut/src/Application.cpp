#include "Common.hpp"
#include "Application.hpp"

namespace Kumo {

    static const std::vector<const char*> ValidationLayers {
        "VK_LAYER_KHRONOS_validation"
    };

    Application::Application()
        : m_window(nullptr)
        , m_instance()
        , m_debug_messenger()
        , m_debug_messenger_create_info()
        , m_physical_device(VK_NULL_HANDLE)
        , m_device()
        , m_surface()
        , m_graphics_queue()
        , m_present_queue()
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

        m_window = glfwCreateWindow(WindowWidth, WindowHeight,
            "Vulkan", nullptr, nullptr);

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
    }

    void Application::RunLoop() {
        while (!glfwWindowShouldClose(m_window)) {
            glfwPollEvents();
        }
    }

    void Application::Cleanup() {
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
        const QueueFamilyIndices queue_family_indices =
            FindQueueFamilies(m_physical_device);
        const float queue_priorities[] { 1.0f };
        std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
        std::set<UInt32> unique_families {
            queue_family_indices.GraphicsFamily.value(),
            queue_family_indices.PresentFamily.value()
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
            0,
            nullptr,
            &features
        };
        if (vkCreateDevice(m_physical_device, &device_create_info, nullptr,
                &m_device) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create logical device.");
        }
        vkGetDeviceQueue(m_device, queue_family_indices.GraphicsFamily.value(),
            0, &m_graphics_queue);
        vkGetDeviceQueue(m_device, queue_family_indices.PresentFamily.value(),
            0, &m_present_queue);
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
        return queue_family_indices.IsComplete();
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