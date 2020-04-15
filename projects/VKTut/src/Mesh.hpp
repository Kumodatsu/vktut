#pragma once

#include <vulkan/vulkan.h>

#include "Vertex.hpp"

namespace Kumo {

    struct Mesh {
        using Index = UInt32;
        static constexpr VkIndexType IndexType = VK_INDEX_TYPE_UINT32;

        std::vector<Vertex> Vertices;
        std::vector<Index>  Indices;
    };

}
