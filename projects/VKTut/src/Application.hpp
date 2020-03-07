#pragma once

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

namespace Kumo {

    class Application {
    public:
        Application();
        ~Application();

        void Run();
    private:
        inline static constexpr int WindowWidth  = 800;
        inline static constexpr int WindowHeight = 600;

        GLFWwindow* m_window;

        VkInstance       m_instance;
        VkPhysicalDevice m_physical_device; // implicitly destroyed with instance

        // Debug members
        VkDebugUtilsMessengerCreateInfoEXT m_debug_messenger_create_info;
        VkDebugUtilsMessengerEXT           m_debug_messenger;

        void InitializeWindow();
        void InitializeVulkan();
        void RunLoop();
        void Cleanup();

        void CreateInstance();
        void SelectPhysicalDevice();

        void SetupDebugMessenger();

        bool AreLayersSupported(
            const std::vector<const char*>& layers) const;
        std::vector<const char*> GetRequiredExtensions() const;
        bool IsPhysicalDeviceSuitable(const VkPhysicalDevice& device,
            VkPhysicalDeviceProperties& properties,
            VkPhysicalDeviceFeatures& features) const;

        inline static constexpr VkDebugUtilsMessageSeverityFlagBitsEXT
            VulkanLogSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
        static VKAPI_ATTR VkBool32 VKAPI_CALL VulkanDebugCallback(
            VkDebugUtilsMessageSeverityFlagBitsEXT      severity,
            VkDebugUtilsMessageTypeFlagsEXT             type,
            const VkDebugUtilsMessengerCallbackDataEXT* data,
            void*
        );
    };

}