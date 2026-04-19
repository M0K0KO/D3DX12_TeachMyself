#pragma once
#include <memory>
#include "ComponentPool.h"
#include "View.h"
#include <unordered_map>
#include <typeindex>

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
            if (pool.second && pool.second->Has(e))
                pool.second->Remove(e);
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

    template<typename T>
    bool Has(Entity e) const
    {
        const auto* pool = TryGetPool<T>();
        return pool && pool->Has(e);
    }

    template<typename T1, typename T2, typename... Rest>
    bool Has(Entity e) const
    {
        return Has<T1>(e) && Has<T2>(e) && (Has<Rest>(e) && ...);
    }

private:
    template<typename T>
    const ComponentPool<T>* TryGetPool() const
    {
        auto it = pools.find(std::type_index(typeid(T)));
        return (it != pools.end()) ? static_cast<const ComponentPool<T>*>(it->second.get()) : nullptr;
    }

    template<typename T>
    ComponentPool<T>& GetPool()
    {
        auto type = std::type_index(typeid(T));

        auto it = pools.find(type);
        if (it == pools.end())
        {
            auto ptr = std::make_unique<ComponentPool<T>>();
            auto* raw = ptr.get();
            pools.emplace(type, std::move(ptr));
            return *raw;
        }

        return *static_cast<ComponentPool<T>*>(it->second.get());
    }

private:
    std::unordered_map<std::type_index, std::unique_ptr<IComponentPool>> pools;
};