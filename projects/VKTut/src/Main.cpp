#include "Common.hpp"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

int main(int argc, char** argv) {
    if (!glfwInit()) {
        std::cout << "Failed to initialize GLFW." << std::endl;
        return -1;
    }
    GLFWwindow* window = glfwCreateWindow(640, 480, "Window", nullptr, nullptr);
    if (!window) {
        std::cout << "Failed to create window." << std::endl;
        glfwTerminate();
        return -1;
    }

    const glm::vec3 u(0.0f, 1.0f, 2.0f);
    const glm::vec3 v(2.0f, 1.0f, 0.0f);
    const glm::vec3 w = u + v;
    std::cout << w.y << std::endl;

    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    std::cout << extensionCount << " extensions supported" << std::endl;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}
