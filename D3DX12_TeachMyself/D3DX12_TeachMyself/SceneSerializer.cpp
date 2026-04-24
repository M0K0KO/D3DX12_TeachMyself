#include "SceneSerializer.h"
#include "Entity.h"
#include "EntityScene.h"
#include "NameComponent.h"
#include "TransformComponent.h"
#include "ComponentSerializer.h"
#include "HirerarchyComponent.h"
#include "SceneFactory.h"
#include <fstream>

bool SceneSerializer::Save(EntityScene& scene, const std::filesystem::path& path)
{
	json root;
	root["version"] = kVersion;
	root["entities"] = json::array();

	std::unordered_map<Entity, int> idMap;
	int counter = 0;

	auto& registry = scene.GetRegistry();

	std::function<void(Entity, int)> visit = [&](Entity e, int parentLocalId) 
		{
			int localId = counter++;
			idMap[e] = localId;

			json entry;
			entry["id"] = localId;
			entry["parent"] = parentLocalId;

			if (registry.Has<NameComponent>(e))
			{
				entry["name"] = registry.Get<NameComponent>(e).name;
			}
			else
			{
				entry["name"] = "";
			}

			if (registry.Has<TransformComponent>(e))
			{
				entry["transform"] = MokoSerialize::ToJson(registry.Get<TransformComponent>(e));
			}

			root["entities"].push_back(entry);

			if (registry.Has<HierarchyComponent>(e))
			{
				const auto& h = registry.Get<HierarchyComponent>(e);
				Entity child = h.firstChild;
				while (child != INVALID_ENTITY)
				{
					visit(child, localId);
					const auto& ch = registry.Get<HierarchyComponent>(child);
					child = ch.nextSibling;
				}
			}

			if (registry.Has<DirectionalLightComponent>(e))
			{
				entry["directionalLight"] = MokoSerialize::ToJson(registry.Get<DirectionalLightComponent>(e));
			}

			if (registry.Has<CameraComponent>(e))
			{
				entry["camera"] = MokoSerialize::ToJson(registry.Get<CameraComponent>(e));
			}

			if (registry.Has<MeshRendererComponent>(e))
			{
				entry["meshRenderer"] = MokoSerialize::ToJson(registry.Get<MeshRendererComponent>(e));
			}
		};

	Entity sceneRoot = scene.GetRoot();
	if (registry.Has<HierarchyComponent>(sceneRoot))
	{
		const auto& h = registry.Get<HierarchyComponent>(sceneRoot);
		Entity child = h.firstChild;

		while (child != INVALID_ENTITY)
		{
			visit(child, -1);
			const auto& ch = registry.Get<HierarchyComponent>(child);
			child = ch.nextSibling;
		}
	}

	std::ofstream out(path);
	if (!out.is_open())
	{
		MOKOLOG_ERROR("SceneSerializer::Save failed to open : {}", path.string());
		return false;
	}
	out << root.dump(2);

	MOKOLOG_INFO("Saved Scene in [{}]", path.string());
	return true;
}

bool SceneSerializer::Load(EntityScene& scene, const std::filesystem::path& path)
{
	std::ifstream in(path);
	if (!in.is_open())
	{
		MOKOLOG_ERROR("SceneSerializer::Load failed to open: {}", path.string());
		return false;
	}

	json root;
	try
	{
		in >> root;  
	}
	catch (const json::parse_error& e)
	{
		MOKOLOG_ERROR("SceneSerializer::Load parse error: {}", e.what());
		return false;
	}

	if (!root.contains("version") || root["version"].get<int>() != kVersion)
	{
		MOKOLOG_ERROR("SceneSerializer::Load version mismatch");
		return false;
	}

	if (!root.contains("entities") || !root["entities"].is_array())
	{
		MOKOLOG_ERROR("SceneSerializer::Load invalid format");
		return false;
	}

	scene.Clear();

	auto& registry = scene.GetRegistry();
	const auto& entries = root["entities"];

	std::vector<Entity> localMap;
	localMap.reserve(entries.size());

	for (const auto& entry : entries)
	{
		Entity e = scene.CreateSceneEntity();
		localMap.push_back(e);

		if (entry.contains("name"))
		{
			registry.Get<NameComponent>(e).name = entry["name"].get<std::string>();
		}

		if (entry.contains("transform"))
		{
			TransformComponent tmp{};
			MokoSerialize::FromJson(entry["transform"], tmp);
			Transform::SetPosition(registry, e, tmp.position);
			Transform::SetRotation(registry, e, tmp.rotation);
			Transform::SetScale(registry, e, tmp.scale);
		}

		if (entry.contains("directionalLight"))
		{
			auto& dl = registry.Add<DirectionalLightComponent>(e);
			MokoSerialize::FromJson(entry["directionalLight"], dl);
		}

		if (entry.contains("camera"))
		{
			auto& cam = registry.Add<CameraComponent>(e);
			MokoSerialize::FromJson(entry["camera"], cam);
		}
	}

	for (size_t i = 0; i < entries.size(); ++i)
	{
		int pid = entries[i]["parent"].get<int>();
		Entity parent = (pid == -1) ? INVALID_ENTITY : localMap[pid];
		scene.SetParent(localMap[i], parent);
	}

	MOKOLOG_INFO("Loaded Scene from [{}]", path.string());
	return true;
}
