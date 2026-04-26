#include "stdafx.h"
#include "CameraControllerSystem.h"
#include "MokoMath.h"
#include "TransformComponent.h"
#include "CameraComponent.h"

void CameraControllerSystem::Init(SystemContext& ctx)
{
}

void CameraControllerSystem::Update(EntityScene& scene, float dt, SystemContext& ctx)
{
	cameraEntity = scene.GetMainCamera();

	auto wnd = ctx.window;
	auto input = ctx.input;

	// Gather input
	XMFLOAT3 localMove = { 0, 0, 0 };
	XMFLOAT2 mouseDelta = { 0, 0 };

	if (!wnd->CursorEnabled())
	{
		if (input->IsKeyDown('W')) localMove.z += 1.0f;
		if (input->IsKeyDown('S')) localMove.z -= 1.0f;
		if (input->IsKeyDown('A')) localMove.x -= 1.0f;
		if (input->IsKeyDown('D')) localMove.x += 1.0f;
		if (input->IsKeyDown('R')) localMove.y += 1.0f;
		if (input->IsKeyDown('F')) localMove.y -= 1.0f;

		mouseDelta.x += (float)input->rawDeltaX;
		mouseDelta.y += (float)input->rawDeltaY;
	}
	else
	{
		while (wnd->mouse.ReadRawDelta()) {}
	}

	auto view = scene.GetRegistry().GetView<TransformComponent, CameraComponent>();
	for (auto [e, t, cam] : view)
	{
		if (!cam.isMain) continue;

		cam.yaw = wrap_angle(cam.yaw + mouseDelta.x * mouseSensitivity);
		cam.pitch = std::clamp(cam.pitch + mouseDelta.y * mouseSensitivity,
			-PI * 0.5f * 0.995f, PI * 0.5f * 0.995f);

		XMVECTOR local = XMLoadFloat3(&localMove);
		XMVECTOR lenSq = XMVector3LengthSq(local);
		if (XMVectorGetX(lenSq) > 0.0f)
		{
			local = XMVector3Normalize(local);
		}
		XMMATRIX rot = XMMatrixRotationRollPitchYaw(cam.pitch, cam.yaw, 0.0f);
		XMVECTOR world = XMVector3TransformNormal(local, rot);
		world = XMVectorScale(world, moveSpeed * dt);

		auto currentPos = XMLoadFloat3(&t.position);
		auto resultPos = currentPos + world;

		XMFLOAT3 result;
		XMStoreFloat3(&result, resultPos);

		Transform::SetPosition(scene.GetRegistry(), e, result);
		break;
	}
}
