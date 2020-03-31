#include "Common.hpp"
#include "IO.hpp"

#include <fstream>

namespace Kumo::IO {

    namespace VFS {
        static std::string s_mount_path = ".";

        void Mount(const std::string& path) {
            s_mount_path = path;
        }

        std::string GetPath(const std::string& path) {
            return s_mount_path + "/" + path;
        }
    }

    std::vector<Byte> ReadBinaryFile(const std::string& path) {
        const std::string& vfs_path = VFS::GetPath(path);
        std::ifstream stream(vfs_path, std::ios::ate | std::ios::binary);
        if (!stream.is_open() || stream.bad()) {
            throw std::runtime_error(
                std::string("Failed to open file: ")
                + vfs_path
            );
        }

        const USize file_size = static_cast<USize>(stream.tellg());
        std::vector<Byte> data(file_size);
        stream.seekg(0);
        stream.read(reinterpret_cast<char*>(data.data()), file_size);
        stream.close();

        return data;
    }

}
