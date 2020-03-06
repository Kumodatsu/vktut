#include "Common.hpp"

#include "Application.hpp"

int main(int, char**) {
    Kumo::Application app;

    try {
        app.Run();
    } catch (const std::runtime_error& error) {
        std::cerr << error.what() << std::endl;
        return -1;
    }

    return 0;
}
