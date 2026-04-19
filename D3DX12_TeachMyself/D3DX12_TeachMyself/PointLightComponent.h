#pragma once
#include <DirectXMath.h>
#include "Entity.h"
#include "Registry.h"

using namespace DirectX;

struct PointLightComponent
{
	XMFLOAT3 color{ 1,1,1 };
	float intensity = 1.0f;
	float radius = 10.0f;
	bool castShadow = false;
};