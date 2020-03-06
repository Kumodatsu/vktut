#include "Common.hpp"

#include <GLFW/glfw3.h>

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

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}
