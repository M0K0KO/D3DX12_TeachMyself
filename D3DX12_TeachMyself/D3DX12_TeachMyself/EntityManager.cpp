#include "EntityManager.h"
#include <assert.h>

Entity EntityManager::Create()
{
    Entity e;

    if (!freeIndices.empty())
    {
        e.index = freeIndices.back();
        freeIndices.pop_back();
    }
    else
    {
        e.index = (uint32_t)generations.size();
        generations.push_back(0);
    }

    e.generation = generations[e.index];
    return e;
}

void EntityManager::Destroy(Entity e)
{
    assert(IsValid(e));

    generations[e.index]++;
    freeIndices.push_back(e.index);
}

bool EntityManager::IsValid(Entity e) const
{
    return e.index < generations.size() &&
        generations[e.index] == e.generation;
}