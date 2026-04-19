#pragma once
#include <DirectXMath.h>
#include "Entity.h"

class Registry;

using namespace DirectX;

struct TransformComponent
{
	XMFLOAT3 position{ 0,0,0 };
	XMFLOAT4 rotation{ 0,0,0,1 };
	XMFLOAT3 scale{ 1,1,1 };

	XMFLOAT4X4 localMatrix;
	XMFLOAT4X4 worldMatrix;

	bool dirty = true;
};

namespace Transform
{
	void SetPosition(Registry& reg, Entity e, const XMFLOAT3& v);
	void SetRotation(Registry& reg, Entity e, const XMFLOAT4& q);
	void SetScale(Registry& reg, Entity e, const XMFLOAT3& s);
}
