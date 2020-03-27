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

    struct SwapchainSupportInfo {
        VkSurfaceCapabilitiesKHR        Capabilities = {};
        std::vector<VkSurfaceFormatKHR> Formats;
        std::vector<VkPresentModeKHR>   PresentModes;
    };

    class Application {
    public:
        Application();
        ~Application();

        void Run();
    private:
        inline static constexpr UInt32 WindowWidth  = 800;
        inline static constexpr UInt32 WindowHeight = 600;

        GLFWwindow* m_window;

        VkInstance       m_instance;
        VkPhysicalDevice m_physical_device; // implicitly destroyed with instance
        VkDevice         m_device;
        VkSurfaceKHR     m_surface;

        QueueFamilyIndices m_queue_family_indices;
        VkQueue
            // implicitly destroyed with logical device
            m_graphics_queue,
            m_present_queue;

        VkSwapchainKHR       m_swapchain;
        std::vector<VkImage> m_swapchain_images; // implicitly destroyed with swapchain
        VkFormat             m_swapchain_image_format;
        VkExtent2D           m_swapchain_extent;

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
        void CreateSwapchain();

        bool AreLayersSupported(
            const std::vector<const char*>& layers) const;
        std::vector<const char*> GetRequiredExtensions() const;
        bool IsPhysicalDeviceSuitable(const VkPhysicalDevice& device,
            VkPhysicalDeviceProperties& properties,
            VkPhysicalDeviceFeatures& features) const;
        QueueFamilyIndices FindQueueFamilies(const VkPhysicalDevice& device)
            const;
        bool CheckDeviceExtensionSupport(const VkPhysicalDevice& device,
            const std::vector<const char*>& extensions) const;
        SwapchainSupportInfo QuerySwapchainSupport(
            const VkPhysicalDevice& device, const VkSurfaceKHR& surface) const;
        VkSurfaceFormatKHR SelectSwapchainSurfaceFormat(
            const std::vector<VkSurfaceFormatKHR>& available_formats) const;
        VkPresentModeKHR SelectSwapchainPresentMode(
            const std::vector<VkPresentModeKHR>& available_modes) const;
        VkExtent2D SelectSwapchainExtent(
            const VkSurfaceCapabilitiesKHR& capabilities) const;

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