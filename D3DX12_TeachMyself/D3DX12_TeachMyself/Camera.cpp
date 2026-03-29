#include "Camera.h"

Camera::Camera()
    :
    aspectRatio(1920.0f / 1080.0f),
    pos{ 0.0f, 35.0f, -200.0f },
    pitch{ 0.0f },
    yaw{ 0.0f },
    nearZ{ 0.1f },
    farZ{ 1000.0f },
    fovY{ PI / 4.0f }
{
}

XMMATRIX Camera::GetViewMatrix() const
{
    const XMVECTOR forward = XMVectorSet(
        cosf(pitch) * sinf(yaw),
        sinf(pitch),
        cosf(pitch) * cosf(yaw),
        0.0f
    );

    const XMVECTOR eye = XMLoadFloat3(&pos);
    const XMVECTOR target = XMVectorAdd(eye, forward);
    const XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    return XMMatrixLookAtLH(eye, target, up);
}

XMMATRIX Camera::GetProjectionMatrix() const
{
    return XMMatrixPerspectiveFovLH(fovY, aspectRatio, nearZ, farZ);
}

XMFLOAT3 Camera::GetPos() const
{
	return pos;
}