#include "Camera.h"
#include <algorithm>
#include "MokoMath.h"

Camera::Camera()
    :
    aspectRatio(0.0f),
    pos{ 0.0f, 1.0f, 0.0f },
    pitch{ 0.0f },
    yaw{ 0.0f },
    nearZ{ 0.1f },
    farZ{ 300.0f },
    fovY{ PI / 4.0f }
{
}

XMMATRIX Camera::GetViewMatrix() const
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

XMMATRIX Camera::GetProjectionMatrix() const
{
    return XMMatrixPerspectiveFovLH(fovY, aspectRatio, nearZ, farZ);
}

XMFLOAT3 Camera::GetPos() const
{
	return pos;
}

void Camera::SetAspectRatio(UINT width, UINT height)
{
    aspectRatio = static_cast<float>(width) / static_cast<float>(height);
}

void Camera::Translate(XMFLOAT3 translation)
{
    XMStoreFloat3(&translation, XMVector3Transform(
        XMLoadFloat3(&translation),
        XMMatrixRotationRollPitchYaw(pitch, yaw, 0.0f) *
        XMMatrixScaling(travelSpeed, travelSpeed, travelSpeed)
    ));

    pos = {
        pos.x + translation.x,
        pos.y + translation.y,
        pos.z + translation.z,
    };
}

void Camera::Rotate(float dx, float dy)
{
    yaw = wrap_angle(yaw + dx * rotationSpeed);
    pitch = std::clamp(pitch + dy * rotationSpeed, 0.995f * -PI / 2.0f, 0.995f * PI / 2.0f);
}