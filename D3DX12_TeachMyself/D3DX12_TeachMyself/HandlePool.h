#pragma once
#include "stdafx.h"

template<typename T>
struct Handle
{
	uint32_t index = UINT32_MAX;
	uint32_t generation = 0;

	bool IsValid() const { return index != UINT32_MAX; }
	bool operator== (const Handle&) const = default;
};

// aliases
using MeshHandle	 = Handle<struct MeshTag>;
using MaterialHandle = Handle<struct MaterialTag>;
using TextureHandle  = Handle<struct TextureTag>;

template<typename T, typename Tag>
class HandlePool
{
public:
	using HandleType = Handle<Tag>;

	template<typename... Args>
	HandleType Create(Args&&... args)
	{
		uint32_t idx;
		if (!m_freeList.empty())
		{
			idx = m_freeList.back();
			m_freeList.pop_back();

			m_items[idx] = T(std::forward<Args>(args)...);
			m_alive[idx] = true;
		}
		else
		{
			idx = static_cast<uint32_t>(m_items.size());
			m_items.emplace_back(std::forward<Args>(args)...);
			m_generations.push_back(0);
			m_alive.push_back(true);
		}
		return { idx, m_generations[idx] };
	}

	T* Get(HandleType h)
	{
		if (!IsValid(h)) return nullptr;
		return &m_items[h.index];
	}

	const T* Get(HandleType h) const
	{
		if (!IsValid(h)) return nullptr;
		return &m_items[h.index];
	}

	bool IsValid(HandleType h) const
	{
		return 
			h.IsValid() 
			&& h.index < m_items.size()
			&& m_alive[h.index]
			&& m_generations[h.index] == h.generation;
	}

	const uint32_t Size() const
	{
		return m_items.size();
	}

	void Destroy(HandleType h)
	{
		if (!IsValid(h)) return;
		m_alive[h.index] = false;
		m_generations[h.index]++;
		m_freeList.push_back(h.index);
	}

	template<typename F>
	void ForEach(F&& fn)
	{
		for (uint32_t i = 0; i < m_items.size(); i++)
		{
			if (m_alive[i]) fn(HandleType{ i, m_generations[i] }, m_items[i]);
		}
	}

private:
	std::vector<T>        m_items;
	std::vector<uint32_t> m_generations;
	std::vector<bool>     m_alive; // handle[i] is being used?
	std::vector<uint32_t> m_freeList; // usable slot indexes
};