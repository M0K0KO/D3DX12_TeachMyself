#pragma once
#include <memory>
#include <vector>
#include "ComponentPool.h"
#include "View.h"

class Registry
{
public:
    Registry() = default;
    Registry(const Registry&) = delete;
    Registry& operator=(const Registry&) = delete;
    Registry(Registry&&) = default;
    Registry& operator=(Registry&&) = default;

    template<typename T, typename... Args>
    T& Add(Entity e, Args&&... args)
    {
        auto& pool = GetPool<T>();
        pool.Add(e, std::forward<Args>(args)...);
        return pool.Get(e);
    }

    template<typename T>
    void Remove(Entity e)
    {
        GetPool<T>().Remove(e);
    }

    void RemoveAll(Entity e)
    {
        for (auto& pool : pools)
        {
            if (pool && pool->Has(e))
                pool->Remove(e);
        }
    }

    template<typename T>
    T& Get(Entity e)
    {
        return GetPool<T>().Get(e);
    }

    template<typename T, typename... Rest>
    View<T, Rest...> GetView()
    {
        return View<T, Rest...>(GetPool<T>(), GetPool<Rest>()...);
    }

private:
    template<typename T>
    ComponentPool<T>& GetPool()
    {
        size_t id = GetComponentTypeID<T>();

        if (id >= pools.size())
            pools.resize(id + 1);

        if (!pools[id])
            pools[id] = std::make_unique<ComponentPool<T>>();

        return *static_cast<ComponentPool<T>*>(pools[id].get());
    }

    template<typename T>
    static size_t GetComponentTypeID()
    {
        static size_t id = nextID++;
        return id;
    }

private:
    inline static size_t nextID = 0;
    std::vector<std::unique_ptr<IComponentPool>> pools;
};