#include "font_manager.h"
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <entt/core/hashed_string.hpp>

namespace engine::resource {

FontManager::FontManager() {
    if (!TTF_WasInit() && !TTF_Init()) {
        throw std::runtime_error("FontManager build: TTF_Init failed: " + std::string(SDL_GetError()));
    }
    spdlog::trace("FontManager build successfully.");
}

FontManager::~FontManager() {
    if (!fonts_.empty()) {
        spdlog::debug("FontManager not empty, calling clearFonts to handle cleanup logic.");
        clearFonts();       // 调用 clearFonts 处理清理逻辑
    }
    TTF_Quit();
    spdlog::trace("FontManager destructor successfully.");
}

TTF_Font* FontManager::loadFont(entt::id_type id, int point_size, std::string_view file_path) {
    // 检查点大小是否有效
    if (point_size <= 0) {
        spdlog::error("invalid point size {} for font '{}'.", point_size, file_path);
        return nullptr;
    }

    // 创建映射表的键
    FontKey key = {id, point_size};

    // 首先检查缓存
    auto it = fonts_.find(key);
    if (it != fonts_.end()) {
        return it->second.get();
    }

    // 缓存中不存在，则加载字体
    spdlog::debug("loading font '{}' ({}pt) ...", file_path, point_size);
    TTF_Font* raw_font = TTF_OpenFont(file_path.data(), point_size);
    if (!raw_font) {
        spdlog::error("loading font '{}' ({}pt) failed: {}", file_path, point_size, SDL_GetError());
        return nullptr;
    }

    // 使用 unique_ptr 存储到缓存中
    fonts_.emplace(key, std::unique_ptr<TTF_Font, SDLFontDeleter>(raw_font));
    spdlog::debug("font '{}' (id = {}, {}pt) loaded and cached successfully.", file_path, id, point_size);
    return raw_font;
}

TTF_Font* FontManager::loadFont(entt::hashed_string str_hs, int point_size) {
    return loadFont(str_hs.value(), point_size, str_hs.data());
}

TTF_Font* FontManager::getFont(entt::id_type id, int point_size, std::string_view file_path) {
    FontKey key = {id, point_size};
    auto it = fonts_.find(key);
    if (it != fonts_.end()) {
        return it->second.get();
    }

    // 如果未找到，判断是否提供了file_path
    if (file_path.empty()) {
        spdlog::error("font '{}' (id = {}, {}pt) not found in cache, and no file path provided, return nullptr.", file_path, id, point_size);
        return nullptr;
    }


    spdlog::warn("font '{}' (id = {}, {}pt) not found in cache, trying to load ...", file_path, id, point_size);
    return loadFont(id, point_size, file_path);
}

TTF_Font* FontManager::getFont(entt::hashed_string str_hs, int point_size) {
    return getFont(str_hs.value(), point_size, str_hs.data());
}

void FontManager::unloadFont(entt::id_type id, int point_size) {
    FontKey key = {id, point_size};
    auto it = fonts_.find(key);
    if (it != fonts_.end()) {
        spdlog::debug("unloading font '{}' ({}pt) ...", id, point_size);
        fonts_.erase(it);       // unique_ptr 会处理 TTF_CloseFont
    } else {
        spdlog::warn("font '{}' ({}pt) not found in cache, nothing to unload.", id, point_size);
    }
}

void FontManager::clearFonts() {
    if (!fonts_.empty()) {
        spdlog::debug("clearing all {} cached fonts ...", fonts_.size());
        fonts_.clear();         // unique_ptr 会处理删除
    }
}

} // namespace engine::resource
