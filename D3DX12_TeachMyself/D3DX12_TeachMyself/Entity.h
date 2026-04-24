#pragma once
#include <cstdint>
#include <functional>

struct Entity
{
    uint32_t index;
    uint32_t generation;
    bool operator==(const Entity& o) const { return index == o.index && generation == o.generation; };
};

namespace std
{
    template<>
    struct hash<Entity>
    {
        size_t operator()(const Entity& e) const noexcept
        {
            return hash<uint64_t>{}(uint64_t(e.index) << 32 | e.generation);
        }
    };
}

constexpr Entity INVALID_ENTITY = { UINT32_MAX, UINT32_MAX };
