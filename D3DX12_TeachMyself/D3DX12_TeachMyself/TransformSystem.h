#pragma once
#include "ISystem.h"

class TransformSystem : public ISystem
{
public:
	void Update(SystemContext& ctx) override;

private:
	void UpdateRecursive(EntityScene& scene, Entity entity, const XMMATRIX& parentWorld, bool parentDirty);
};