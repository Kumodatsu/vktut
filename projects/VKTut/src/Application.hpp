#pragma once

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

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

    struct UniformBufferObject {
        glm::mat4 Model;
        glm::mat4 View;
        glm::mat4 Projection;
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

        VkRenderPass          m_render_pass;
        VkDescriptorSetLayout m_descriptor_set_layout;
        VkPipelineLayout      m_pipeline_layout;
        VkPipeline            m_graphics_pipeline;
        VkCommandPool         m_cmd_pool;
        VkDescriptorPool      m_descriptor_pool;
        
        std::vector<VkDescriptorSet>
            m_descriptor_sets; // implicitly destroyed with descriptor pool

        VkDeviceMemory
            m_mem_vertex_buffer,
            m_mem_index_buffer;
        std::vector<VkDeviceMemory>
            m_mems_uniform_buffers;
        VkBuffer
            m_vertex_buffer,
            m_index_buffer;
        std::vector<VkBuffer>
            m_uniform_buffers;

        VkImage        m_texture_image;
        VkDeviceMemory m_mem_texture_image;

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
        void UpdateUniformBuffer(UInt32 current_image);

        void CreateInstance();
        void CreateSurface();
        void SelectPhysicalDevice();
        void CreateLogicalDevice();
        void CreateSwapchain();
        void CreateSwapchainImageViews();
        void CreateRenderPass();
        void CreateDescriptorSetLayout();
        void CreateGraphicsPipeline();
        void CreateFramebuffers();
        void CreateCommandPool();
        void CreateVertexBuffer();
        void CreateIndexBuffer();
        void CreateUniformBuffers();
        void CreateDescriptorPool();
        void CreateDescriptorSets();
        void CreateCommandBuffers();
        void CreateSynchronizationObjects();

        void RecreateSwapchain();
        void CleanupSwapchain();

        void CreateTextureImage(const std::string& path);
        void CreateImage(UInt32 width, UInt32 height, VkFormat format,
            VkImageTiling tiling, VkImageUsageFlags usage,
            VkMemoryPropertyFlags properties, VkImage& out_image,
            VkDeviceMemory& out_memory) const;

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
        void TransitionImageLayout(VkImage image, VkFormat format,
            VkImageLayout old_layout, VkImageLayout new_layout) const;
        void CopyBufferToImage(const VkBuffer& buffer, const VkImage& image,
            UInt32 width, UInt32 height) const;

        void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage_flags,
            VkMemoryPropertyFlags property_flags, VkBuffer& out_buffer,
            VkDeviceMemory& out_memory) const;
        void CopyBuffer(const VkBuffer& src, const VkBuffer& dst,
            VkDeviceSize size) const;
        VkCommandBuffer BeginSingleTimeCommands() const;
        void EndSingleTimeCommands(const VkCommandBuffer& cmd_buffer) const;

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