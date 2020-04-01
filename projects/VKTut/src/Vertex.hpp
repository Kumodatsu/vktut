#pragma once
#include <glm/glm.hpp>

namespace Kumo {

    struct Vertex {
        glm::vec2 Position;
        glm::vec3 Color;

        inline static VkVertexInputBindingDescription GetBindingDescription() {
            return {
                0,
                sizeof(Vertex),
                VK_VERTEX_INPUT_RATE_VERTEX
            };
        }

        inline static std::array<VkVertexInputAttributeDescription, 2>
                GetAttributeDescriptions() {
            return {{
                {0, 0, VK_FORMAT_R32G32_SFLOAT,    offsetof(Vertex, Position)},
                {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, Color)}
            }};
        }
    };

}
