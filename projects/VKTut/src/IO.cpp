#include "Common.hpp"
#include "IO.hpp"

#include <fstream>

namespace Kumo::IO {

    static const std::string s_mount_path = "../..";

    static std::string Mount(const std::string& path) {
        return s_mount_path + "/" + path;
    }

    std::vector<Byte> ReadBinaryFile(const std::string& path) {
        std::ifstream stream(Mount(path), std::ios::ate | std::ios::binary);
        if (!stream.is_open() || stream.bad()) {
            throw std::runtime_error("Failed to open file.");
        }

        const USize file_size = static_cast<USize>(stream.tellg());
        std::vector<Byte> data(file_size);
        stream.seekg(0);
        stream.read(reinterpret_cast<char*>(data.data()), file_size);
        stream.close();

        return data;
    }

}
