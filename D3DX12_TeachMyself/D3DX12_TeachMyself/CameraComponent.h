#pragma once
#include <DirectXMath.h>
#include "Entity.h"
#include "Registry.h"

using namespace DirectX;

struct CameraComponent
{
	float fovY = XM_PIDIV4;
	float aspect = 16.0f / 9.0f;
	float nearZ = 0.1f;
	float farZ = 100.0f;
	float pitch = 0.0f;
	float yaw = 0.0f;
	bool isMain = true;
};

namespace Camera
{
	XMMATRIX GetViewMatrix(const XMFLOAT3& pos, float pitch, float yaw);
	XMMATRIX GetProjectionMatrix(float fovY, float aspect, float nearZ, float farZ);
	XMFLOAT3 GetForward(float pitch, float yaw);
}