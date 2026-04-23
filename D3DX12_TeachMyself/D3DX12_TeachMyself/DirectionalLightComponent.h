#pragma once
#include <DirectXMath.h>
#include "Entity.h"
#include "Registry.h"

using namespace DirectX;

struct DirectionalLightComponent
{
	XMFLOAT3 direction{ 0,-1, 0 };
	XMFLOAT3 color{ 1,1,1 };
	float intensity = 1.0f;
	float ambient = 0.8f;
};