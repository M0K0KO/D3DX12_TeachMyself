#pragma once
#include "json.hpp"
#include <DirectXMath.h>
#include "TransformComponent.h"
#include "NameComponent.h"
#include "DirectionalLightComponent.h"
#include "CameraComponent.h"
#include "MeshRendererComponent.h"

using namespace DirectX;
using json = nlohmann::json;

namespace MokoSerialize
{
	json ToJson(const TransformComponent& t);
	void FromJson(const json& j, TransformComponent& t);

	json ToJson(const NameComponent& n);
	void FromJson(const json& j, NameComponent& n);

	json ToJson(const DirectionalLightComponent& l);
	void FromJson(const json& j, DirectionalLightComponent& l);

	json ToJson(const CameraComponent& c);
	void FromJson(const json& j, CameraComponent& c);

	json ToJson(const MeshRendererComponent& m);
	void FromJson(const json& j, MeshSource& src);
}