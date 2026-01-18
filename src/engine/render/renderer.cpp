#include "renderer.h"
#include "../resource/resource_manager.h"
#include "camera.h"
#include "image.h"
#include <SDL3/SDL.h>
#include <stdexcept> // For std::runtime_error
#include <spdlog/spdlog.h>

namespace engine::render {

// 构造函数: 执行初始化，增加 ResourceManager
Renderer::Renderer(SDL_Renderer* sdl_renderer, engine::resource::ResourceManager* resource_manager)
    : renderer_(sdl_renderer), resource_manager_(resource_manager) 
{
    spdlog::trace("Renderer build successfully.");
    if (!renderer_) {
        throw std::runtime_error("Renderer build failed: provided SDL_Renderer pointer is null.");
    }
    if (!resource_manager_) {
        // ResourceManager 是 drawSprite 所必需的
        throw std::runtime_error("Renderer build failed: provided ResourceManager pointer is null.");
    }
    setDrawColor(0, 0, 0, 255);
    spdlog::trace("Renderer build successfully.");
}

void Renderer::drawSprite(const Camera& camera, const component::Sprite& sprite, const glm::vec2& position, const glm::vec2& size, const float rotation) {
    auto texture = resource_manager_->getTexture(sprite.texture_id_, sprite.texture_path_);
    if (!texture) {
        spdlog::error("unable to get texture for ID {}.", sprite.texture_id_);
        return;
    }

    // 应用相机变换
    glm::vec2 screen_position = camera.worldToScreen(position);

    // 计算目标矩形
    SDL_FRect dest_rect = {
        screen_position.x,
        screen_position.y,
        size.x,
        size.y
    };

    if (!isRectInViewport(camera, dest_rect)) { // 视口裁剪：如果精灵超出视口，则不绘制
        // spdlog::info("sprite {} is out of viewport, ID: {}", sprite.getTextureId(), sprite.getTextureId());
        return;
    }

    SDL_FRect src_rect = {
        sprite.src_rect_.position.x,
        sprite.src_rect_.position.y,
        sprite.src_rect_.size.x,
        sprite.src_rect_.size.y
    };

    // 执行绘制(默认旋转中心为精灵的中心点)
    if (!SDL_RenderTextureRotated(renderer_, texture, &src_rect, &dest_rect, rotation, NULL, sprite.is_flipped_ ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE)) {
        spdlog::error("render rotate texture failed（ID: {}）：{}", sprite.texture_id_, SDL_GetError());
    }   
}

void Renderer::drawUIImage(const Image& image, const glm::vec2& position, const std::optional<glm::vec2>& size) {
    auto texture = resource_manager_->getTexture(image.getTextureId());
    if (!texture) {
        spdlog::error("cannot get texture for ID {}.", image.getTextureId());
        return;
    }

    auto src_rect = getImageSrcRect(image);
    if (!src_rect.has_value()) {
        spdlog::error("cannot get src rect for image ID {}.", image.getTextureId());
        return;
    }

    SDL_FRect dest_rect = {position.x, position.y, 0, 0};   // 首先确定目标矩形的左上角坐标
    if (size.has_value()) {                                 // 如果提供了尺寸，则使用提供的尺寸
        dest_rect.w = size.value().x;
        dest_rect.h = size.value().y;
    } else {                                                // 如果未提供尺寸，则使用纹理的原始尺寸
        dest_rect.w = src_rect.value().w;
        dest_rect.h = src_rect.value().h;
    }

    // 执行绘制(未考虑UI旋转)
    if (!SDL_RenderTextureRotated(renderer_, texture, &src_rect.value(), &dest_rect, 0.0, nullptr, image.isFlipped() ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE)) {
        spdlog::error("draw UI Image failed for ID {}: {}", image.getTextureId(), SDL_GetError());
    }
}

void Renderer::setDrawColor(Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
    if (!SDL_SetRenderDrawColor(renderer_, r, g, b, a)) {
        spdlog::error("set draw color failed: {}", SDL_GetError());
    }
}

void Renderer::setDrawColorFloat(float r, float g, float b, float a) {
    if (!SDL_SetRenderDrawColorFloat(renderer_, r, g, b, a)) {
        spdlog::error("set draw color failed: {}", SDL_GetError());
    }
}

void Renderer::clearScreen() {
    setDrawColorFloat(background_color_.r, background_color_.g, background_color_.b, background_color_.a);
    if (!SDL_RenderClear(renderer_)) {
        spdlog::error("clear screen failed: {}", SDL_GetError());
    }
}

void Renderer::drawUIFilledRect(const engine::utils::Rect &rect, const engine::utils::FColor &color)
{
    setDrawColorFloat(color.r, color.g, color.b, color.a);
    SDL_FRect sdl_rect = {rect.position.x, rect.position.y, rect.size.x, rect.size.y};
    if (!SDL_RenderFillRect(renderer_, &sdl_rect)) {
        spdlog::error("draw filled rect failed: {}", SDL_GetError());
    }
    setDrawColorFloat(0.0f, 0.0f, 0.0f, 1.0f);
}

void Renderer::present()
{
    SDL_RenderPresent(renderer_);
}

std::optional<SDL_FRect> Renderer::getImageSrcRect(const Image& image)
{
    SDL_Texture* texture = resource_manager_->getTexture(image.getTextureId());
    if (!texture) {
        spdlog::error("cannot get texture for ID {}.", image.getTextureId());
        return std::nullopt;
    }

    auto src_rect = image.getSourceRect();
    if (src_rect.has_value()) {     // 如果Image中存在指定rect，则判断尺寸是否有效
        if (src_rect.value().size.x <= 0 || src_rect.value().size.y <= 0) {
            spdlog::error("invalid source rect size, ID: {}, path: {}", image.getTextureId(), image.getTexturePath());
            return std::nullopt;
        }
        return SDL_FRect{
            src_rect.value().position.x, 
            src_rect.value().position.y, 
            src_rect.value().size.x, 
            src_rect.value().size.y
        };
    } else {                        // 否则获取纹理尺寸并返回整个纹理大小
        SDL_FRect result = {0, 0, 0, 0};
        if (!SDL_GetTextureSize(texture, &result.w, &result.h)) {
            spdlog::error("cannot get texture size, ID: {}, path: {}", image.getTextureId(), image.getTexturePath());
            return std::nullopt;
        }
        return result;
    }
}

bool Renderer::isRectInViewport(const Camera& camera, const SDL_FRect &rect) {
    glm::vec2 viewport_size = camera.getViewportSize();
    return rect.x + rect.w >= 0 && rect.x <= viewport_size.x &&     // 相当于 AABB碰撞检测
           rect.y + rect.h >= 0 && rect.y <= viewport_size.y;
}

} // namespace engine::render
