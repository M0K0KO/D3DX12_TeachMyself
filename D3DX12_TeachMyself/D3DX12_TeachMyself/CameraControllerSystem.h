#pragma once
#include "ISystem.h"

class CameraControllerSystem : public ISystem
{
public:
    void Init(EntityScene& scene) override;
    void Update(EntityScene& scene, float dt,
        const SystemContext& ctx) override;

private:
    Entity cameraEntity = INVALID_ENTITY;
    float moveSpeed = 6.0f;
    float mouseSensitivity = 0.0025f;
};