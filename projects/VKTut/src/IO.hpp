#pragma once

namespace Kumo::IO {

    namespace VFS {
        void Mount(const std::string& path);
        std::string GetPath(const std::string& path);
    }

    std::vector<Byte> ReadBinaryFile(const std::string& path);

}
