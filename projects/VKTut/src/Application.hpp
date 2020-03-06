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

        VkInstance  m_instance;

        void InitializeWindow();
        void InitializeVulkan();
        void RunLoop();
        void Cleanup();

        void CreateInstance();

        bool AreLayersSupported(
            const std::vector<const char*>& layers) const;
    };

}