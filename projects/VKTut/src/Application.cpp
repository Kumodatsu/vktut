#include "Common.hpp"
#include "Application.hpp"

namespace Kumo {

    Application::Application()
        : m_window(nullptr)
        , m_instance()
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
        CreateInstance();
    }

    void Application::RunLoop() {
        while (!glfwWindowShouldClose(m_window)) {
            glfwPollEvents();
        }
    }

    void Application::Cleanup() {
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

        UInt32 n_glfw_extensions;
        const char** glfw_extensions =
            glfwGetRequiredInstanceExtensions(&n_glfw_extensions);

        UInt32 n_extensions = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &n_extensions,
            nullptr);
        std::vector<VkExtensionProperties> extensions(n_extensions);
        vkEnumerateInstanceExtensionProperties(nullptr, &n_extensions,
            extensions.data());
        std::cout << "Available extensions:" << std::endl;
        for (const auto& extension : extensions)
            std::cout << "\t" << extension.extensionName << std::endl;

        for (UIndex i = 0; i < n_glfw_extensions; i++) {
            const char* extension_name = glfw_extensions[i];
            const auto r = std::find_if(extensions.begin(), extensions.end(),
                [extension_name] (VkExtensionProperties extension) {
                    return strcmp(extension_name, extension.extensionName) == 0;
                }
            );
            if (r == extensions.end()) {
                throw std::runtime_error(
                    "Not all Vulkan extensions required by GLFW are supported."
                );
            }
        }

        std::vector<const char*> layers;

        KUMO_DEBUG_ONLY {
            const std::vector<const char*> validation_layers {
                "VK_LAYER_KHRONOS_validation"
            };
            if (!AreLayersSupported(validation_layers)) {
                throw std::runtime_error(
                    "Validation layers are not available."
                );
            }
            layers.insert(layers.end(), validation_layers.begin(),
                validation_layers.end());
        }

        const VkInstanceCreateInfo create_info {
            VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            nullptr,
            0,
            &app_info,
            static_cast<UInt32>(layers.size()),
            layers.data(),
            n_glfw_extensions,
            glfw_extensions
        };

        if (vkCreateInstance(&create_info, nullptr, &m_instance) != VK_SUCCESS)
            throw std::runtime_error("Failed to create Vulkan instance.");
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

}