#include "stdafx.h"
#include "EntityScene.h"
#include "MokoMath.h"
#include "HirerarchyComponent.h"
#include "TransformComponent.h"
#include "CameraComponent.h"
#include "NameComponent.h"

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
	MOKO_ASSERT(registry.Has<HierarchyComponent>(child));
	MOKO_ASSERT(child != GetRoot());

	if (newParent == INVALID_ENTITY)
	{
		newParent = GetRoot();
	}

	MOKO_ASSERT(registry.Has<HierarchyComponent>(newParent));
	MOKO_ASSERT(child != newParent);

	auto& cHier = registry.Get<HierarchyComponent>(child);
	if (!(cHier.parent == INVALID_ENTITY))
	{
		DetachFromParent(child);
	}

	auto& pHier = registry.Get<HierarchyComponent>(newParent);
	cHier.parent = newParent;
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

void EntityScene::Clear()
{
	Entity root = GetRoot();

	Entity child = registry.Get<HierarchyComponent>(root).firstChild;
	while (!(child == INVALID_ENTITY))
	{
		Entity next = registry.Get<HierarchyComponent>(child).nextSibling;
		DestroyEntity(child);
		child = next;
	}

	auto& rootHier = registry.Get<HierarchyComponent>(root);
	rootHier.firstChild = INVALID_ENTITY;
	rootHier.prevSibling = INVALID_ENTITY;
	rootHier.nextSibling = INVALID_ENTITY;
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


