#pragma once
#include <cstdint>

struct Entity
{
    uint32_t index;
    uint32_t generation;
    bool operator==(const Entity& o) const { return index == o.index && generation == o.generation; };
};

constexpr Entity INVALID_ENTITY = { UINT32_MAX, UINT32_MAX };
