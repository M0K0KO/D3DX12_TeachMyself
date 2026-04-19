#include "EntityScene.h"
#include "MokoMath.h"
#include "HirerarchyComponent.h"
#include "TransformComponent.h"
#include "CameraComponent.h"
#include "NameComponent.h"
#include <cassert>

EntityScene::EntityScene()
{
	rootEntity = CreateEntity("Root");
	registry.Add<TransformComponent>(rootEntity);
	registry.Add<HierarchyComponent>(rootEntity);
}

Entity EntityScene::CreateEntity(const std::string& name)
{
	Entity e = entityManager.Create();
	registry.Add<NameComponent>(e).name = name;
	return e;
}

Entity EntityScene::CreateSceneEntity(const std::string& name)
{
	Entity e = entityManager.Create();
	registry.Add<TransformComponent>(e);
	registry.Add<HierarchyComponent>(e);
	registry.Add<NameComponent>(e).name = name;

	if (!(rootEntity == INVALID_ENTITY))
	{
		SetParent(e, rootEntity);
	}

	return e;
}

void EntityScene::DestroyEntity(Entity e)
{
	auto& hier = registry.Get<HierarchyComponent>(e);
	Entity c = hier.firstChild;
	while (!(c == INVALID_ENTITY))
	{
		Entity next = registry.Get<HierarchyComponent>(c).nextSibling;
		DestroyEntity(c);
		c = next;
	}

	DetachFromParent(e);

	registry.RemoveAll(e);
	entityManager.Destroy(e);
}

void EntityScene::SetParent(Entity child, Entity newParent)
{
	assert(registry.Has<HierarchyComponent>(child));
	assert(registry.Has<HierarchyComponent>(newParent));

	auto& cHier = registry.Get<HierarchyComponent>(child);

	if (!(cHier.parent == INVALID_ENTITY))
	{
		DetachFromParent(child);
	}

	cHier.parent = newParent;
	auto& pHier = registry.Get<HierarchyComponent>(newParent);

	cHier.nextSibling = pHier.firstChild;
	cHier.prevSibling = INVALID_ENTITY;

	if (!(pHier.firstChild == INVALID_ENTITY))
	{
		registry.Get<HierarchyComponent>(pHier.firstChild).prevSibling = child;
	}
	pHier.firstChild = child;

	registry.Get<TransformComponent>(child).dirty = true;
}

void EntityScene::DetachFromParent(Entity e)
{
	auto& hier = registry.Get<HierarchyComponent>(e);

	if (hier.parent == INVALID_ENTITY) return;

	auto& pHier = registry.Get<HierarchyComponent>(hier.parent);

	if (pHier.firstChild == e)
	{
		pHier.firstChild = hier.nextSibling;
	}

	if (!(hier.prevSibling == INVALID_ENTITY))
	{
		registry.Get<HierarchyComponent>(hier.prevSibling).nextSibling = hier.nextSibling;
	}
	if (!(hier.nextSibling == INVALID_ENTITY))
	{
		registry.Get<HierarchyComponent>(hier.nextSibling).prevSibling = hier.prevSibling;
	}

	hier.parent = INVALID_ENTITY;
	hier.prevSibling = INVALID_ENTITY;
	hier.nextSibling = INVALID_ENTITY;
}

Entity EntityScene::GetMainCamera()
{
	auto view = registry.GetView<CameraComponent>();
	for (auto [e, cam] : view)
	{
		if (cam.isMain) return e;
	}

	return INVALID_ENTITY;
}

void EntityScene::SetSceneAABB(XMFLOAT3 min, XMFLOAT3 max)
{
	sceneAABBMax = max;
	sceneAABBMin = min;
}

XMFLOAT3 EntityScene::GetSceneAABBMin() const
{
	return sceneAABBMin;
}

XMFLOAT3 EntityScene::GetSceneAABBMax() const
{
	return sceneAABBMax;
}


