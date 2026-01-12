#include "physics_component.h"
#include "transform_component.h"
#include "../object/game_object.h"
#include "../physics/physics_engine.h"
#include <spdlog/spdlog.h>

namespace engine::component {

PhysicsComponent::PhysicsComponent(engine::physics::PhysicsEngine* physics_engine, bool use_gravity, float mass)
    : physics_engine_(physics_engine), mass_(mass >= 0.0f ? mass : 1.0f), use_gravity_(use_gravity) {
    if (!physics_engine_) {
        spdlog::error("PhysicsComponent: the PhysicsEngine pointer is null! This component will be invalid.");
    }
    spdlog::trace("PhysicsComponent: constructed with mass: {}, use_gravity: {}", mass_, use_gravity_);
}

void PhysicsComponent::init() {
    if (!owner_) {
        spdlog::error("PhysicsComponent: the owner is null! This component will be invalid.");
        return;
    }
    if (!physics_engine_) {
        spdlog::error("PhysicsComponent: the PhysicsEngine is null! This component will be invalid.");
        return;
    }
    transform_ = owner_->getComponent<TransformComponent>();
    if (!transform_) {
        spdlog::warn("PhysicsComponent: the owner GameObject '{}' requires a TransformComponent, but none was found.", owner_->getName());
    }
    // 注册到PhysicsEngine
    physics_engine_->registerComponent(this);
    spdlog::trace("PhysicsComponent: initialized.");
}

void PhysicsComponent::clean() 
{
    physics_engine_->unregisterComponent(this);
    spdlog::trace("物理组件清理完成。");
}

} // namespace engine::component