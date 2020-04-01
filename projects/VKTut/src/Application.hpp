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
        Application() = default;
        ~Application();

        void Run();
    private:
        inline static constexpr UInt32 WindowWidth  = 800;
        inline static constexpr UInt32 WindowHeight = 600;

        inline static constexpr USize MaxFramesInFlight = 2;

        USize m_current_frame = 0;

        GLFWwindow* m_window              = nullptr;
        bool        m_framebuffer_resized = false;

        VkInstance       m_instance;
        VkPhysicalDevice m_physical_device; // implicitly destroyed with instance
        VkDevice         m_device;
        VkSurfaceKHR     m_surface;

        QueueFamilyIndices m_queue_family_indices;
        VkQueue
            // implicitly destroyed with logical device
            m_graphics_queue,
            m_present_queue;

        VkSwapchainKHR             m_swapchain;
        std::vector<VkImage>       m_swapchain_images; // implicitly destroyed with swapchain
        std::vector<VkImageView>   m_swapchain_image_views;
        VkFormat                   m_swapchain_image_format;
        VkExtent2D                 m_swapchain_extent;
        std::vector<VkFramebuffer> m_swapchain_framebuffers;

        VkRenderPass     m_render_pass;
        VkPipelineLayout m_pipeline_layout;
        VkPipeline       m_graphics_pipeline;
        VkCommandPool    m_cmd_pool;

        VkDeviceMemory
            m_mem_vertex_buffer;
        VkBuffer
            m_vertex_buffer;

        std::vector<VkCommandBuffer> m_cmd_buffers; // implicitly destroyed with command pool

        std::vector<VkSemaphore>
            m_sems_image_available,
            m_sems_render_finished;
        std::vector<VkFence>
            m_fens_in_flight,
            m_fens_images_in_flight;

        // Debug members
        VkDebugUtilsMessengerCreateInfoEXT m_debug_messenger_create_info;
        VkDebugUtilsMessengerEXT           m_debug_messenger;

        void InitializeWindow();
        void InitializeVulkan();
        void RunLoop();
        void Cleanup();

        void DrawFrame();

        void CreateInstance();
        void CreateSurface();
        void SelectPhysicalDevice();
        void CreateLogicalDevice();
        void CreateSwapchain();
        void CreateSwapchainImageViews();
        void CreateRenderPass();
        void CreateGraphicsPipeline();
        void CreateFramebuffers();
        void CreateCommandPool();
        void CreateVertexBuffer();
        void CreateCommandBuffers();
        void CreateSynchronizationObjects();

        void RecreateSwapchain();
        void CleanupSwapchain();

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
        VkShaderModule CreateShaderModule(const std::vector<Byte>& bytecode)
            const;
        UInt32 SelectMemoryType(UInt32 type_filter,
            VkMemoryPropertyFlags properties) const;
        void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage_flags,
            VkMemoryPropertyFlags property_flags, VkBuffer& out_buffer,
            VkDeviceMemory& out_memory) const;

        void SetupDebugMessenger();

        inline static constexpr VkDebugUtilsMessageSeverityFlagBitsEXT
            VulkanLogSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
        static VKAPI_ATTR VkBool32 VKAPI_CALL VulkanDebugCallback(
            VkDebugUtilsMessageSeverityFlagBitsEXT      severity,
            VkDebugUtilsMessageTypeFlagsEXT             type,
            const VkDebugUtilsMessengerCallbackDataEXT* data,
            void*
        );
        static void GLFWFramebufferResizeCallback(
            GLFWwindow* window,
            int width,
            int height
        );
    };

}