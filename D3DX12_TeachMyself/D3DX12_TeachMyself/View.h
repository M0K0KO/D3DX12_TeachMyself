#pragma once
#include <tuple>
#include "ComponentPool.h"

template<typename T, typename... Rest>
class View
{
public:
    View(ComponentPool<T>& primary, ComponentPool<Rest>&... rest)
        : primary(&primary), rest(&rest...)
    {}

    class Iterator
    {
    public:
        Iterator(View* view, size_t index)
            : view(view), index(index)
        {
            SkipInvalid();
        }

        Iterator& operator++()
        {
            index++;
            SkipInvalid();
            return *this;
        }

        auto operator*() const
        {
            Entity e = view->primary->GetEntities()[index];
            return GetTuple(e, std::index_sequence_for<Rest...>{});
        }

        bool operator!=(const Iterator& other) const
        {
            return index != other.index;
        }

    private:
        template<size_t... I>
        auto GetTuple(Entity e, std::index_sequence<I...>) const
        {
            return std::tuple<Entity, T&, Rest&...>(
                e,
                view->primary->Get(e),
                std::get<I>(view->rest)->Get(e)...
            );
        }

        void SkipInvalid()
        {
            auto& entities = view->primary->GetEntities();

            while (index < entities.size())
            {
                if (view->AllHave(entities[index]))
                    break;
                index++;
            }
        }

        View* view;
        size_t index;
    };

    Iterator begin() { return Iterator(this, 0); }
    Iterator end() { return Iterator(this, primary->Size()); }

private:
    ComponentPool<T>* primary;
    std::tuple<ComponentPool<Rest>*...> rest;

    bool AllHave(Entity e) const
    {
        return std::apply([e](auto*... pools) {
            return (pools->Has(e) && ...);
            }, rest);
    }
};