#include "TransformSystem.h"
#include "components.h"
#include "MokoMath.h"

void TransformSystem::Update(EntityScene& scene, float dt, const SystemContext& ctx)
{
	UpdateRecursive(scene, scene.GetRoot(), XMMatrixIdentity());
}

void TransformSystem::UpdateRecursive(EntityScene & scene, Entity entity, const XMMATRIX & parentWorld)
{
	auto& t = scene.GetRegistry().Get<TransformComponent>(entity);

	XMMATRIX local = GetLocalMatrix(t.position, t.rotation, t.scale);
	XMMATRIX world = local * parentWorld;

	XMStoreFloat4x4(&t.locallMatrix, local);
	XMStoreFloat4x4(&t.worldMatrix, world);

	auto& hier = scene.GetRegistry().Get<HierarchyComponent>(entity);
	Entity c = hier.firstChild;
	while (!(c == INVALID_ENTITY))
	{
		UpdateRecursive(scene, c, world);
		c = scene.GetRegistry().Get<HierarchyComponent>(c).nextSibling;
	}
}
