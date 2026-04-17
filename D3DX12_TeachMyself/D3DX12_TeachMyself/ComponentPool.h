#pragma once
#include <vector>
#include <assert.h>
#include "Entity.h"
#include "IComponentPool.h"

template<typename T>
class ComponentPool : public IComponentPool
{
public:
    template<typename... Args>
    void Add(Entity e, Args&&... args)
    {
        assert(!Has(e));

        size_t index = dense.size();

        dense.emplace_back(std::forward<Args>(args)...);
        denseToEntity.push_back(e);

        if (e.index >= sparse.size())
            sparse.resize(e.index + 1, INVALID);

        sparse[e.index] = index;
    }

    void Remove(Entity e) override
    {
        assert(Has(e));

        size_t index = sparse[e.index];
        size_t lastIndex = dense.size() - 1;

        if (index == lastIndex)
        {
            dense.pop_back();
            denseToEntity.pop_back();
            sparse[e.index] = INVALID;
            return;
        }

        dense[index] = std::move(dense[lastIndex]);
        Entity moved = denseToEntity[lastIndex];

        denseToEntity[index] = moved;
        sparse[moved.index] = index;

        dense.pop_back();
        denseToEntity.pop_back();

        sparse[e.index] = INVALID;
    }

    T& Get(Entity e)
    {
        assert(Has(e));
        return dense[sparse[e.index]];
    }

    bool Has(Entity e) const override
    {
        return e.index < sparse.size() && sparse[e.index] != INVALID;
    }

    size_t Size() const override { return dense.size(); }

    const std::vector<T>& GetDense() const { return dense; }
    const std::vector<Entity>& GetEntities() const { return denseToEntity; }

private:
    static constexpr size_t INVALID = (size_t)-1;

    std::vector<size_t> sparse;
    std::vector<T> dense;
    std::vector<Entity> denseToEntity;
};