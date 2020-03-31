#include "Common.hpp"

#include "IO.hpp"
#include "Application.hpp"

int main(int argc, char** argv) {
    Kumo::Application app;
    
    if (argc >= 2)
        Kumo::IO::VFS::Mount(argv[1]);
    
    try {
        app.Run();
    } catch (const std::runtime_error& error) {
        std::cerr << error.what() << std::endl;
        return -1;
    }

    return 0;
}
