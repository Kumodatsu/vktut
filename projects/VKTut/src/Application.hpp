#pragma once

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

namespace Kumo {

    struct QueueFamilyIndices {
        std::optional<UInt32> GraphicsFamily = std::nullopt;
        std::optional<UInt32> PresentFamily  = std::nullopt;

        inline bool IsComplete() const {
            return GraphicsFamily.has_value()
                && PresentFamily.has_value();
        }
    };

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
        VkDevice         m_device;
        VkSurfaceKHR     m_surface;
        VkQueue
            // implicitly destroyed with logical device
            m_graphics_queue,
            m_present_queue;

        // Debug members
        VkDebugUtilsMessengerCreateInfoEXT m_debug_messenger_create_info;
        VkDebugUtilsMessengerEXT           m_debug_messenger;

        void InitializeWindow();
        void InitializeVulkan();
        void RunLoop();
        void Cleanup();

        void CreateInstance();
        void CreateSurface();
        void SelectPhysicalDevice();
        void CreateLogicalDevice();

        bool AreLayersSupported(
            const std::vector<const char*>& layers) const;
        std::vector<const char*> GetRequiredExtensions() const;
        bool IsPhysicalDeviceSuitable(const VkPhysicalDevice& device,
            VkPhysicalDeviceProperties& properties,
            VkPhysicalDeviceFeatures& features) const;
        QueueFamilyIndices FindQueueFamilies(const VkPhysicalDevice& device)
            const;

        void SetupDebugMessenger();

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