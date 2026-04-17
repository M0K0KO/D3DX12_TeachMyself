#pragma once
#include "Entity.h"
#include <DirectXMath.h>
#include <string>
#include "RHITypes.h"

using namespace DirectX;

struct TransformComponent
{
	XMFLOAT3 position{ 0,0,0 };
	XMFLOAT4 rotation{ 0,0,0,1 };
	XMFLOAT3 scale{ 1,1,1 };

	XMFLOAT4X4 locallMatrix;
	XMFLOAT4X4 worldMatrix;
};

struct HierarchyComponent
{
	Entity parent		= INVALID_ENTITY;
	Entity firstChild	= INVALID_ENTITY;
	Entity nextSibling	= INVALID_ENTITY;
	Entity prevSibling	= INVALID_ENTITY;
};

struct NameComponent
{
	std::string name;
};

struct MeshRendererComponent
{
	// TODO
	//MeshHandle mesh;
	//MaterialHandle material;

	BufferHandle vertexBuffer;
	BufferHandle indexBuffer;
	uint32_t indexOffset = 0;
	uint32_t indexCount = 0;
	GPUMaterial material;
	XMFLOAT3 aabbMin = { FLT_MAX,  FLT_MAX,  FLT_MAX };
	XMFLOAT3 aabbMax = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
	bool visible = true;
};

struct DirectionalLightComponent
{
	XMFLOAT3 direction{ 0,-1, 0 };
	XMFLOAT3 color{ 1,1,1 };
	float intensity = 1.0f;
	float ambient = 0.05f;
};

struct PointLightComponent
{
	XMFLOAT3 color{ 1,1,1 };
	float intensity = 1.0f;
	float radius = 10.0f;
	bool castShadow = false;
};

struct CameraComponent
{
	float fovY = XM_PIDIV4;
	float aspect = 16.0f / 9.0f;
	float nearZ = 0.1f;
	float farZ = 1000.0f;
	float pitch = 0.0f;
	float yaw = 0.0f;
	bool isMain = true;
};

namespace CameraMath
{
	inline XMMATRIX GetViewMatrix(const XMFLOAT3& pos, float pitch, float yaw)
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

	inline XMMATRIX GetProjectionMatrix(float fovY, float aspect, float nearZ, float farZ)
	{
		return XMMatrixPerspectiveFovLH(fovY, aspect, nearZ, farZ);
	}

	inline XMFLOAT3 GetForward(float pitch, float yaw)
	{
		return XMFLOAT3(
			cosf(pitch) * sinf(yaw),
			-sinf(pitch),
			cosf(pitch) * cosf(yaw)
		);
	}
}
