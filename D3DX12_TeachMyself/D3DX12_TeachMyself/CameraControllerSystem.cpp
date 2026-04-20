#include "CameraControllerSystem.h"
#include "MokoMath.h"
#include <algorithm>
#include "TransformComponent.h"
#include "CameraComponent.h"

void CameraControllerSystem::Init(SystemContext& ctx)
{
	cameraEntity = ctx.scene->GetMainCamera();
}

void CameraControllerSystem::Update(SystemContext& ctx)
{
	auto wnd = ctx.window;
	auto input = ctx.input;
	auto scene = ctx.scene;

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

	auto view = scene->GetRegistry().GetView<TransformComponent, CameraComponent>();
	for (auto [e, t, cam] : view)
	{
		if (!cam.isMain) continue;

		cam.yaw = wrap_angle(cam.yaw + mouseDelta.x * mouseSensitivity);
		cam.pitch = std::clamp(cam.pitch + mouseDelta.y * mouseSensitivity,
			-PI * 0.5f * 0.995f, PI * 0.5f * 0.995f);

		XMVECTOR local = XMLoadFloat3(&localMove);
		XMMATRIX rot = XMMatrixRotationRollPitchYaw(cam.pitch, cam.yaw, 0.0f);
		XMVECTOR world = XMVector3TransformNormal(local, rot);
		world = XMVectorScale(world, moveSpeed * ctx.dt);

		auto currentPos = XMLoadFloat3(&t.position);
		auto resultPos = currentPos + world;

		XMFLOAT3 result;
		XMStoreFloat3(&result, resultPos);

		Transform::SetPosition(scene->GetRegistry(), e, result);
		break;
	}
}
