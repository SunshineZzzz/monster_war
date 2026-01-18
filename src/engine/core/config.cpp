#include "config.h"
#include <fstream>
#include <filesystem>
#include <nlohmann/json.hpp>
#include "spdlog/spdlog.h"

namespace engine::core {

Config::Config(std::string_view filepath)
{
    loadFromFile(filepath);
}

bool Config::loadFromFile(std::string_view filepath) {
    auto path = std::filesystem::path(filepath);    // 将string_view转换为文件路径 (或std::sring)
    std::ifstream file(path);                       // ifstream 不支持std::string_view 构造
    if (!file.is_open()) {
        spdlog::warn("config file '{}' not found. Using default settings and create a default config file.", filepath);
        if (!saveToFile(filepath)) {
            spdlog::error("unable to create default config file '{}'.", filepath);
            return false;
        }
        return false; // 文件不存在，使用默认值
    }

    try {
        nlohmann::json j;
        file >> j;
        fromJson(j);
        spdlog::info("successfully load config from '{}'.", filepath);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("error when read config file '{}': {}. Using default settings.", filepath, e.what());
    }
    return false;
}

bool Config::saveToFile(std::string_view filepath) {
    auto path = std::filesystem::path(filepath);    // 将string_view转换为文件路径
    std::ofstream file(path);
    if (!file.is_open()) {
        spdlog::error("unable to open config file '{}' for writing.", filepath);
        return false;
    }

    try {
        nlohmann::ordered_json j = toJson();
        file << j.dump(4);
        spdlog::info("successfully save config to '{}'.", filepath);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("error when write config file '{}': {}", filepath, e.what());
    }
    return false;
}

void Config::fromJson(const nlohmann::json& j) {
    if (j.contains("window")) {
        const auto& window_config = j["window"];
        window_title_ = window_config.value("title", window_title_);
        window_width_ = window_config.value("width", window_width_);
        window_height_ = window_config.value("height", window_height_);
        window_scale_ = window_config.value("window_scale", window_scale_);
        window_logical_scale_ = window_config.value("logical_scale", window_logical_scale_);
        window_resizable_ = window_config.value("resizable", window_resizable_);
    }
    if (j.contains("graphics")) {
        const auto& graphics_config = j["graphics"];
        vsync_enabled_ = graphics_config.value("vsync", vsync_enabled_);
    }
    if (j.contains("performance")) {
        const auto& perf_config = j["performance"];
        target_fps_ = perf_config.value("target_fps", target_fps_);
        if (target_fps_ < 0) {
            spdlog::warn("target_fps cannot be negative. Set to 0 (unlimited).");
            target_fps_ = 0;
        }
    }
    if (j.contains("audio")) {
        const auto& audio_config = j["audio"];
        music_volume_ = audio_config.value("music_volume", music_volume_);
        sound_volume_ = audio_config.value("sound_volume", sound_volume_);
    }

    // 从 JSON 加载 input_mappings
    if (j.contains("input_mappings") && j["input_mappings"].is_object()) {
        const auto& mappings_json = j["input_mappings"];
        try {
            // 直接尝试从 JSON 对象转换为 map<string, vector<string>>
            auto input_mappings = mappings_json.get<std::unordered_map<std::string, std::vector<std::string>>>();
            // 如果成功，则将 input_mappings 移动到 input_mappings_
            input_mappings_ = std::move(input_mappings);
            spdlog::trace("successfully load input mappings from config.");
        } catch (const std::exception& e) {
            spdlog::warn("config load warning: error when parse 'input_mappings' from config. Using default mappings. Error: {}", e.what());
        }
    } else {
        spdlog::trace("config trace: 'input_mappings' section not found or not an object. Using default mappings defined in header file.");
    }
}

nlohmann::ordered_json Config::toJson() const {
    return nlohmann::ordered_json{
        {"window", {
            {"title", window_title_},
            {"width", window_width_},
            {"height", window_height_},
            {"window_scale", window_scale_},
            {"logical_scale", window_logical_scale_},
            {"resizable", window_resizable_}
        }},
        {"graphics", {
            {"vsync", vsync_enabled_}
        }},
        {"performance", {
            {"target_fps", target_fps_}
        }},
        {"audio", {
            {"music_volume", music_volume_},
            {"sound_volume", sound_volume_}
        }},
        {"input_mappings", input_mappings_}
    };
}

} // namespace engine::core 