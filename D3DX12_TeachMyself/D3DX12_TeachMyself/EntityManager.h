#pragma once
#include <vector>
#include "Entity.h"

class EntityManager
{
public:
    EntityManager() = default;
    Entity Create();
    void Destroy(Entity e);
    bool IsValid(Entity e) const;

private:
    std::vector<uint32_t> generations;
    std::vector<uint32_t> freeIndices;
};