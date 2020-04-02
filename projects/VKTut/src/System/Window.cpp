#include "Common.hpp"
#include "Window.hpp"

namespace Kumo {

    UInt32 Window::s_n_windows = 0;

    Window::Window(const std::string& title, const WindowExtent& extent)
        : Window(title, extent.Width, extent.Height)
    { }

    Window::Window(const std::string& title, UInt32 width, UInt32 height)
        : m_window(nullptr)
    {
        if (glfwInit() == GLFW_FALSE)
            throw std::runtime_error("Failed to initialize GLFW.");
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE,  GLFW_TRUE);
        m_window = glfwCreateWindow(
            static_cast<int>(width),
            static_cast<int>(height),
            title.c_str(),
            nullptr,
            nullptr
        );
        if (!m_window)
            throw std::runtime_error("Failed to create window.");
        s_n_windows++;
        glfwSetWindowUserPointer(m_window, this);
        glfwSetFramebufferSizeCallback(m_window, CallbackFramebufferResized);
    }

    Window::~Window() {
        glfwDestroyWindow(m_window);
        s_n_windows--;
        if (s_n_windows == 0)
            glfwTerminate();
    }

    WindowExtent Window::GetExtent() const {
        int width, height;
        glfwGetFramebufferSize(m_window, &width, &height);
        return {static_cast<UInt32>(width), static_cast<UInt32>(height)};
    }

    bool Window::ShouldClose() const {
        return static_cast<bool>(glfwWindowShouldClose(m_window));
    }

    void Window::Update() {
        glfwPollEvents();
    }

    void Window::CallbackFramebufferResized(GLFWwindow* handle, int width,
            int height) {
        Window* window = reinterpret_cast<Window*>(
            glfwGetWindowUserPointer(handle)
        );
    }

}
