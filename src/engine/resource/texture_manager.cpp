#include "texture_manager.h"
#include <SDL3_image/SDL_image.h> // 用于 IMG_LoadTexture, IMG_Init, IMG_Quit
#include <spdlog/spdlog.h>
#include <stdexcept>

namespace engine::resource {
TextureManager::TextureManager(SDL_Renderer* renderer) : renderer_(renderer) {
    if (!renderer_) {
        // 关键错误，无法继续，抛出异常 （它将由catch语句捕获（位于GameApp），并进行处理）
        throw std::runtime_error("TextureManager build: renderer pointer is null.");
    }
    // SDL3中不再需要手动调用IMG_Init/IMG_Quit
    spdlog::trace("TextureManager build successfully.");
}

SDL_Texture* TextureManager::loadTexture(std::string_view file_path) {
    // 检查是否已加载
    auto it = textures_.find(std::string(file_path));   // 键为std::string, 因此需要转换
    if (it != textures_.end()) {
        return it->second.get();
    }

    // 如果没加载则尝试加载纹理
    SDL_Texture* raw_texture = IMG_LoadTexture(renderer_, file_path.data());    // 通过.data()获取const char*的指针

    // 载入纹理时，设置纹理缩放模式为最邻近插值(必不可少，否则TileLayer渲染中会出现边缘空隙/模糊)
    if (!SDL_SetTextureScaleMode(raw_texture, SDL_SCALEMODE_NEAREST)) {
        spdlog::warn("cannot set texture scale mode to nearest interpolation.");
    }

    if (!raw_texture) {
        spdlog::error("failed to load texture '{}': {}", file_path, SDL_GetError());
        return nullptr;
    }

    // 使用带有自定义删除器的 unique_ptr 存储加载的纹理
    textures_.emplace(file_path, std::unique_ptr<SDL_Texture, SDLTextureDeleter>(raw_texture));
    spdlog::debug("successfully loaded and cached texture: {}", file_path);

    return raw_texture;
}

SDL_Texture* TextureManager::getTexture(std::string_view file_path) {
    // 查找现有纹理
    auto it = textures_.find(std::string(file_path));
    if (it != textures_.end()) {
        return it->second.get();
    }

    // 如果未找到，尝试加载它
    spdlog::warn("texture '{}' not found in cache, trying to load it.", file_path);
    return loadTexture(file_path);
}

glm::vec2 TextureManager::getTextureSize(std::string_view file_path) {
    // 获取纹理
    SDL_Texture* texture = getTexture(file_path);
    if (!texture) {
        spdlog::error("failed to get texture: {}", file_path);
        return glm::vec2(0);
    }

    // 获取纹理尺寸
    glm::vec2 size;
    if (!SDL_GetTextureSize(texture, &size.x, &size.y)) {
        spdlog::error("failed to get texture size: {}", file_path);
        return glm::vec2(0);
    }
    return size;
}

void TextureManager::unloadTexture(std::string_view file_path) {
    auto it = textures_.find(std::string(file_path));
    if (it != textures_.end()) {
        spdlog::debug("successfully unloaded texture: {}", file_path);
        textures_.erase(it); // unique_ptr 通过自定义删除器处理删除
    } else {
        spdlog::warn("texture '{}' not found in cache, cannot unload.", file_path);
    }
}

void TextureManager::clearTextures() {
    if (!textures_.empty()) {
        spdlog::debug("successfully cleared all {} cached textures.", textures_.size());
        textures_.clear(); // unique_ptr 处理所有元素的删除
    }
}

} // namespace engine::resource
