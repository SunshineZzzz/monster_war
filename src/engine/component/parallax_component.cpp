#include "parallax_component.h"
#include "transform_component.h"
#include "../render/renderer.h"
#include "../render/camera.h"
#include "../render/sprite.h"
#include "../object/game_object.h"
#include "../core/context.h"
#include <spdlog/spdlog.h>

namespace engine::component {

ParallaxComponent::ParallaxComponent(std::string_view texture_id, glm::vec2 scroll_factor, glm::bvec2 repeat)
    : sprite_(texture_id), scroll_factor_(std::move(scroll_factor)), repeat_(std::move(repeat))
{
    spdlog::trace("ParallaxComponent: constructed with texture_id: {}", texture_id);
}

void ParallaxComponent::init() {
    if (!owner_) {
        spdlog::error("ParallaxComponent: the owner is null! This component will be invalid.");
        return;
    }
    transform_ = owner_->getComponent<TransformComponent>();
    if (!transform_) {
        spdlog::error("ParallaxComponent: the owner GameObject '{}' requires a TransformComponent, but none was found.", owner_->getName());
        return;
    }
}

void ParallaxComponent::render(engine::core::Context& context) {
    if (is_hidden_ || !transform_) {
        return;
    }
    // 直接调用视差滚动绘制函数
    context.getRenderer().drawParallax(context.getCamera(), sprite_, transform_->getPosition(), scroll_factor_, repeat_, transform_->getScale());  
}

} // namespace engine::component 