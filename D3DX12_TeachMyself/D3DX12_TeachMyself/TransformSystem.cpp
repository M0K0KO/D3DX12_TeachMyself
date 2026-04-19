#include "TransformSystem.h"
#include "MokoMath.h"
#include "TransformComponent.h"
#include "HirerarchyComponent.h"

void TransformSystem::Update(EntityScene& scene, float dt, const SystemContext& ctx)
{
	UpdateRecursive(scene, scene.GetRoot(), XMMatrixIdentity(), false);
}

void TransformSystem::UpdateRecursive(EntityScene & scene, Entity entity, const XMMATRIX & parentWorld, bool parentDirty)
{
	auto& t = scene.GetRegistry().Get<TransformComponent>(entity);

	bool needRecompute = parentDirty || t.dirty;
	XMMATRIX world;

	if (needRecompute)
	{
		XMMATRIX local;
		if (t.dirty)
		{
			local = GetLocalMatrix(t.position, t.rotation, t.scale);
			XMStoreFloat4x4(&t.localMatrix, local);
			t.dirty = false;
		}
		else
		{
			local = XMLoadFloat4x4(&t.localMatrix);
		}
		world = local * parentWorld;
		XMStoreFloat4x4(&t.worldMatrix, world);
	}
	else
	{
		world = XMLoadFloat4x4(&t.worldMatrix);
	}

	auto& hier = scene.GetRegistry().Get<HierarchyComponent>(entity);
	Entity c = hier.firstChild;
	while (!(c == INVALID_ENTITY))
	{
		UpdateRecursive(scene, c, world, needRecompute);
		c = scene.GetRegistry().Get<HierarchyComponent>(c).nextSibling;
	}
}
