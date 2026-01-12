#include "font_manager.h"
#include <spdlog/spdlog.h>
#include <stdexcept>

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

TTF_Font* FontManager::loadFont(std::string_view file_path, int point_size) {
    // 检查点大小是否有效
    if (point_size <= 0) {
        spdlog::error("invalid point size {} for font '{}'.", point_size, file_path);
        return nullptr;
    }

    // 创建映射表的键
    FontKey key = {std::string(file_path), point_size};

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
    spdlog::debug("font '{}' ({}pt) loaded and cached successfully.", file_path, point_size);
    return raw_font;
}

TTF_Font* FontManager::getFont(std::string_view file_path, int point_size) {
    FontKey key = {std::string(file_path), point_size};
    auto it = fonts_.find(key);
    if (it != fonts_.end()) {
        return it->second.get();
    }

    spdlog::warn("font '{}' ({}pt) not found in cache, trying to load ...", file_path, point_size);
    return loadFont(file_path, point_size);
}

void FontManager::unloadFont(std::string_view file_path, int point_size) {
    FontKey key = {std::string(file_path), point_size};
    auto it = fonts_.find(key);
    if (it != fonts_.end()) {
        spdlog::debug("unloading font '{}' ({}pt) ...", file_path, point_size);
        fonts_.erase(it);       // unique_ptr 会处理 TTF_CloseFont
    } else {
        spdlog::warn("font '{}' ({}pt) not found in cache, nothing to unload.", file_path, point_size);
    }
}

void FontManager::clearFonts() {
    if (!fonts_.empty()) {
        spdlog::debug("clearing all {} cached fonts ...", fonts_.size());
        fonts_.clear();         // unique_ptr 会处理删除
    }
}

} // namespace engine::resource
