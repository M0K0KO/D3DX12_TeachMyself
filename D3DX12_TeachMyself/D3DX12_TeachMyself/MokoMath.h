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

inline XMFLOAT3 QuatToEuler(const XMFLOAT4& q)
{
	XMVECTOR quat = XMLoadFloat4(&q);

	XMMATRIX m = XMMatrixRotationQuaternion(quat);

	XMFLOAT3 euler;

	euler.x = asinf(-m.r[2].m128_f32[1]);

	if (cosf(euler.x) > 1e-6f)
	{
		euler.y = atan2f(m.r[2].m128_f32[0], m.r[2].m128_f32[2]);
		euler.z = atan2f(m.r[0].m128_f32[1], m.r[1].m128_f32[1]);
	}
	else
	{
		euler.y = 0.0f;
		euler.z = atan2f(-m.r[1].m128_f32[0], m.r[0].m128_f32[0]);
	}

	return euler;
}

inline XMFLOAT4 EulerToQuat(const XMFLOAT3& euler)
{
	XMVECTOR q = XMQuaternionRotationRollPitchYaw(
		euler.x, // pitch
		euler.y, // yaw
		euler.z  // roll
	);

	XMFLOAT4 result;
	XMStoreFloat4(&result, q);
	return result;
}

inline XMFLOAT3 Normalize3(const XMFLOAT3& v)
{
	const XMVECTOR vec = XMVector3Normalize(XMLoadFloat3(&v));
	XMFLOAT3 out;
	XMStoreFloat3(&out, vec);
	return out;
}

inline float LengthSq3(const XMFLOAT3& v)
{
	return v.x * v.x + v.y * v.y + v.z * v.z;
}
