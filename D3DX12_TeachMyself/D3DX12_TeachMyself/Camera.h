#pragma once
#include "stdafx.h"

constexpr float PI = 3.14159265f;
constexpr double PI_D = 3.1415926535897932;

using namespace DirectX;

class Camera
{
public:
	Camera();
	XMMATRIX GetViewMatrix() const;
	XMMATRIX GetProjectionMatrix() const;
	XMFLOAT3 GetPos() const;

private:
	XMFLOAT3 pos;

	float pitch;
	float yaw;

	float nearZ;
	float farZ;
	float fovY;
	float aspectRatio;
};