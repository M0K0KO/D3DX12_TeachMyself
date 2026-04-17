#pragma once
#include "Entity.h"

struct IComponentPool
{
    virtual ~IComponentPool() = default;
    virtual void Remove(Entity e) = 0;
    virtual bool Has(Entity e) const = 0;
    virtual size_t Size() const = 0;
};