#pragma once
#include <math.h>

constexpr float PI = 3.14159265f;
constexpr double PI_D = 3.1415926535897932;

template <typename T>
T wrap_angle(T theta)
{
	constexpr T twoPi = (T)2 * (T)PI_D;
	const T mod = fmod(theta, twoPi);
	if (mod > (T)PI_D)
	{
		return mod - twoPi;
	}
	else if (mod < -(T)PI_D)
	{
		return mod + twoPi;
	}
	return mod;
}

inline float DegToRad(float deg)
{
	return deg * XM_PI / 180.0f;
}

inline XMVECTOR EulerToQuaternion(const XMFLOAT3& euler, bool isRadian = false)
{
	if (isRadian)
	{
		return XMQuaternionRotationRollPitchYaw(
			euler.x, // pitch (X)
			euler.y, // yaw   (Y)
			euler.z  // roll  (Z)
		);
	}
	else
	{
		return XMQuaternionRotationRollPitchYaw(
			DegToRad(euler.x), // pitch (X)
			DegToRad(euler.y), // yaw   (Y)
			DegToRad(euler.z)  // roll  (Z)
		);
	}
}

inline XMMATRIX GetLocalMatrix(const XMFLOAT3& position,
	const XMFLOAT3& euler,
	const XMFLOAT3& scale)
{
	XMVECTOR p = XMLoadFloat3(&position);
	XMVECTOR q = EulerToQuaternion(euler);
	q = XMQuaternionNormalize(q);
	XMVECTOR s = XMLoadFloat3(&scale);

	XMMATRIX S = XMMatrixScalingFromVector(s);
	XMMATRIX R = XMMatrixRotationQuaternion(q);
	XMMATRIX T = XMMatrixTranslationFromVector(p);

	return S * R * T;
}

inline XMMATRIX GetLocalMatrix(const XMFLOAT3& position,
	const XMFLOAT4& quaternion,
	const XMFLOAT3& scale)
{
	XMVECTOR p = XMLoadFloat3(&position);
	XMVECTOR q = XMLoadFloat4(&quaternion);
	XMVECTOR s = XMLoadFloat3(&scale);

	XMMATRIX S = XMMatrixScalingFromVector(s);
	XMMATRIX R = XMMatrixRotationQuaternion(q);
	XMMATRIX T = XMMatrixTranslationFromVector(p);

	return S * R * T;
}

