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
	EntityScene();

	Entity CreateEntity(const std::string& name = "Entity");
	Entity CreateSceneEntity(const std::string& name = "SceneEntity");
	void DestroyEntity(Entity e);

	void SetParent(Entity child, Entity parent);

	Entity GetMainCamera();

	EntityManager& GetEntityManager() { return entityManager; };
	Registry& GetRegistry() { return registry; };
	Entity GetRoot() const { return rootEntity; };

	void Clear();

	void SetSceneAABB(XMFLOAT3 min, XMFLOAT3 max);
	XMFLOAT3 GetSceneAABBMin() const;
	XMFLOAT3 GetSceneAABBMax() const;
	
private:
	void DetachFromParent(Entity e);

private:
	EntityManager entityManager;
	Registry registry;
	Entity rootEntity;
};