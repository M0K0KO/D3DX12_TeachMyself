#include "ComponentSerializer.h"

namespace
{
	inline json Vec3ToJson(const XMFLOAT3& v)
	{
		return json::array({ v.x, v.y, v.z });
	}

	inline XMFLOAT3 JsonToVec3(const json& j)
	{
		if (!j.is_array() || j.size() != 3) return { 0, 0, 0 };
		return {
			j[0].get<float>(),
			j[1].get<float>(),
			j[2].get<float>(),
		};
	}

	inline json QuatToJson(const XMFLOAT4& q)
	{
		return json::array({ q.x, q.y, q.z, q.w });
	}

	inline XMFLOAT4 JsonToQuat(const json& j)
	{
		if (!j.is_array() || j.size() != 4) return { 0, 0, 0, 1 };
		return {
			j[0].get<float>(),
			j[1].get<float>(),
			j[2].get<float>(),
			j[3].get<float>(),
		};
	}
}


namespace MokoSerialize
{
	json ToJson(const TransformComponent& t)
	{
		return
		{
			{"pos", Vec3ToJson(t.position)},
			{"rot", QuatToJson(t.rotation)},
			{"scale", Vec3ToJson(t.scale)},
		};
	}
	void FromJson(const json& j, TransformComponent& t)
	{
		if (j.contains("pos"))   t.position = JsonToVec3(j["pos"]);
		if (j.contains("rot"))   t.rotation = JsonToQuat(j["rot"]);
		if (j.contains("scale")) t.scale = JsonToVec3(j["scale"]);
	}

	json ToJson(const NameComponent& n)
	{
		return
		{
			{"name", n.name},
		};
	}
	void FromJson(const json& j, NameComponent& n)
	{
		if (j.contains("name")) n.name = j["name"];
	}

	json ToJson(const DirectionalLightComponent& l)
	{
		return {
			{"direction", Vec3ToJson(l.direction)},
			{"color",     Vec3ToJson(l.color)},
			{"intensity", l.intensity},
			{"ambient",   l.ambient},
		};
	}
	void FromJson(const json& j, DirectionalLightComponent& l)
	{
		if (j.contains("direction")) l.direction = JsonToVec3(j["direction"]);
		if (j.contains("color"))     l.color = JsonToVec3(j["color"]);
		if (j.contains("intensity")) l.intensity = j["intensity"].get<float>();
		if (j.contains("ambient"))   l.intensity = j["ambient"].get<float>();
	}

	json ToJson(const CameraComponent& c)
	{
		return {
			{"fovY",   c.fovY},
			{"nearZ",  c.nearZ},
			{"farZ",   c.farZ},
			{"pitch",  c.pitch},
			{"yaw",	   c.yaw},
			{"isMain", c.isMain},
		};
	}
	void FromJson(const json& j, CameraComponent& c)
	{
		if (j.contains("fovY"))   c.fovY   = j["fovY"].get<float>();
		if (j.contains("nearZ"))  c.nearZ  = j["nearZ"].get<float>();
		if (j.contains("farZ"))   c.farZ   = j["farZ"].get<float>();
		if (j.contains("pitch"))  c.pitch  = j["pitch"].get<float>();
		if (j.contains("yaw"))    c.yaw    = j["yaw"].get<float>();
		if (j.contains("isMain")) c.isMain = j["isMain"].get<bool>();
	}

	json ToJson(const MeshRendererComponent& m)
	{
		if (m.source.type == MeshSource::Type::None) return {};

		json j;
		j["sourceType"] = (m.source.type == MeshSource::Type::GLTF) ? "gltf" : "builtin";
		j["path"] = m.source.path;
		j["submeshIndex"] = m.source.submeshIndex;

		return j;
	}
	void FromJson(const json& j, MeshSource& src)
	{
		std::string t = j.value("sourceType", "none");
		if (t == "gltf")    src.type = MeshSource::Type::GLTF;
		else if (t == "builtin") src.type = MeshSource::Type::Builtin;
		else                src.type = MeshSource::Type::None;

		src.path = j.value("path", "");
		src.submeshIndex = j.value("submeshIndex", 0);
	}
}
