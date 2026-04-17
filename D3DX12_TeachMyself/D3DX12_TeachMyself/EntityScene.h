#pragma once
#include "Entity.h"
#include <string>
#include "EntityManager.h"
#include "Registry.h"
#include <DirectXMath.h>

using namespace DirectX;

class EntityScene
{
	XMFLOAT3 sceneAABBMin, sceneAABBMax;
public:
	void Initialize();

	Entity CreateEntity(const std::string& name = "Entity");
	void DestroyEntity(Entity e);

	void SetParent(Entity child, Entity parent);
	void UpdateTransforms();

	Entity GetMainCamera();

	EntityManager& GetEntityManager() { return entityManager; };
	Registry& GetRegistry() { return registry; };

	void SetSceneAABB(XMFLOAT3 min, XMFLOAT3 max);
	XMFLOAT3 GetSceneAABBMin() const;
	XMFLOAT3 GetSceneAABBMax() const;
	
private:
	void DetachFromParent(Entity e);
	void UpdateTransformRecursive(Entity e, const XMMATRIX& parentWorld);

private:
	EntityManager entityManager;
	Registry registry;
	Entity rootEntity = INVALID_ENTITY;
};