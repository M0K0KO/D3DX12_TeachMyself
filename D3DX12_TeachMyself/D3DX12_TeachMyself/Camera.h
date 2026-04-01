#pragma once
#include "stdafx.h"

using namespace DirectX;

class Camera
{
public:
	Camera();
	XMMATRIX GetViewMatrix() const;
	XMMATRIX GetProjectionMatrix() const;
	XMFLOAT3 GetPos() const;
	void SetAspectRatio(UINT width, UINT height);
	void Translate(XMFLOAT3 translation);
	void Rotate(float dx, float dy);

private:
	XMFLOAT3 pos;

	float pitch;
	float yaw;

	float nearZ;
	float farZ;
	float fovY;
	float aspectRatio;

	static constexpr float travelSpeed = 8.0f;
	static constexpr float rotationSpeed = 0.002f;
};