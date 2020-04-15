#pragma once

#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
#undef GLM_ENABLE_EXPERIMENTAL

namespace Kumo {

    struct Vertex {
        glm::vec3 Position;
        glm::vec3 Color;
        glm::vec2 TextureCoords;

        inline static VkVertexInputBindingDescription GetBindingDescription() {
            return {
                0,
                sizeof(Vertex),
                VK_VERTEX_INPUT_RATE_VERTEX
            };
        }

        inline static std::array<VkVertexInputAttributeDescription, 3>
                GetAttributeDescriptions() {
            return {{
                {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, Position)},
                {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, Color)},
                {2, 0, VK_FORMAT_R32G32_SFLOAT,    offsetof(Vertex, TextureCoords)}
            }};
        }
    };

    inline bool operator == (const Vertex& u, const Vertex& v) {
        return u.Position      == v.Position
            && u.Color         == v.Color
            && u.TextureCoords == v.TextureCoords;
    }

}

namespace std {

    template <> struct hash<::Kumo::Vertex> {
        size_t operator () (const ::Kumo::Vertex& v) const {
            const auto hash_pos = hash<glm::vec3>()(v.Position);
            const auto hash_col = hash<glm::vec3>()(v.Color) << 1;
            const auto hash_tex = hash<glm::vec2>()(v.TextureCoords) << 1;
            return ((hash_pos ^ hash_col) >> 1) ^ hash_tex;
        }
    };

}
