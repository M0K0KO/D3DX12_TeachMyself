#include "stdafx.h"
#include "TransformComponent.h"
#include "Registry.h"
#include "MokoMath.h"

namespace Transform
{
	void SetPosition(Registry& reg, Entity e, const XMFLOAT3& v)
	{
		auto& t = reg.Get<TransformComponent>(e);
		t.position = v;
		t.dirty = true;
	}
	void SetRotation(Registry& reg, Entity e, const XMFLOAT3& euler)
	{
		auto& t = reg.Get<TransformComponent>(e);
		t.rotation = EulerToQuat(euler);
		t.dirty = true;
	}
	void SetRotation(Registry& reg, Entity e, const XMFLOAT4& q)
	{
		auto& t = reg.Get<TransformComponent>(e);
		t.rotation = NormalizeSafeQuat(q);
		t.dirty = true;
	}
	void SetScale(Registry& reg, Entity e, const XMFLOAT3& s)
	{
		auto& t = reg.Get<TransformComponent>(e);
		t.scale = s;
		t.dirty = true;
	}
}