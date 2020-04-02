#pragma once

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

namespace Kumo {

    struct WindowExtent {
        UInt32 Width;
        UInt32 Height;

        inline explicit operator VkExtent2D() {
            return { Width, Height };
        }
    };

    class Window {
    public:
        Window(const std::string& title, const WindowExtent& extent);
        Window(const std::string& title, UInt32 width, UInt32 height);
        ~Window();

        WindowExtent GetExtent() const;
        bool ShouldClose() const;

        void Update();
    private:
        GLFWwindow* m_window;

        static UInt32 s_n_windows;
        static void CallbackFramebufferResized(GLFWwindow* window, int width,
            int height);
    };

}
