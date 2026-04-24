#include "stdafx.h"
#include "CameraComponent.h"

namespace Camera
{
	XMMATRIX GetViewMatrix(const XMFLOAT3& pos, float pitch, float yaw)
	{
		const XMVECTOR forward = XMVectorSet(
			cosf(pitch) * sinf(yaw),
			-sinf(pitch),
			cosf(pitch) * cosf(yaw),
			0.0f
		);

		const XMVECTOR eye = XMLoadFloat3(&pos);
		const XMVECTOR target = XMVectorAdd(eye, forward);
		const XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

		return XMMatrixLookAtLH(eye, target, up);
	}

	XMMATRIX GetProjectionMatrix(float fovY, float aspect, float nearZ, float farZ)
	{
		return XMMatrixPerspectiveFovLH(fovY, aspect, nearZ, farZ);
	}

	XMFLOAT3 GetForward(float pitch, float yaw)
	{
		return XMFLOAT3(
			cosf(pitch) * sinf(yaw),
			-sinf(pitch),
			cosf(pitch) * cosf(yaw)
		);
	}
}