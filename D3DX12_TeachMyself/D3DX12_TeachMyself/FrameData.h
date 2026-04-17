#pragma once
#include <DirectXMath.h>
#include <vector>

using namespace DirectX;

#define CASCADE_COUNT 4
#define MAX_POINT_LIGHTS 8

struct PointLightData
{
	XMFLOAT3 Position;
	float Radius;
	XMFLOAT3 Color;
	float Intensity;
};

struct FrameData
{
	// PerFrame Data
	XMFLOAT4X4 ViewMatrix;
	XMFLOAT4X4 ProjMatrix;
	XMFLOAT3 CameraPos;
	XMFLOAT2 ScreenSize;

	// Directional Light Data
	XMFLOAT3 DirectionalLightDir;
	XMFLOAT3 DirectionalLightColor;
	float DirectionalLightIntensity;
	float Ambient;

	// Point Light Data
	int PointLightCount;
	std::vector<PointLightData> PointLights;
};

