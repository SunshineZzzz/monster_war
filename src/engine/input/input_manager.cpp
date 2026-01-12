#include "input_manager.h"
#include "../core/config.h"
#include <stdexcept>
#include <SDL3/SDL.h>
#include <spdlog/spdlog.h>
#include <glm/vec2.hpp>


namespace engine::input {

InputManager::InputManager(SDL_Renderer* sdl_renderer, const engine::core::Config* config): sdl_renderer_(sdl_renderer) {
    if (!sdl_renderer_) {
        spdlog::error("input manager: SDL_Renderer is null pointer");
        throw std::runtime_error("input manager: SDL_Renderer is null pointer");
    }
    initializeMappings(config);
    // 获取初始鼠标位置
    float x, y;
    SDL_GetMouseState(&x, &y);
    mouse_position_ = {x, y};
    spdlog::trace("initial mouse position: ({}, {})", mouse_position_.x, mouse_position_.y);
}

// --- 更新和事件处理 ---

void InputManager::update() {
    // 1. 根据上一帧的值更新默认的动作状态
    for (auto& [action_name, state] : action_states_) {
        if (state == ActionState::PRESSED_THIS_FRAME) {
            state = ActionState::HELD_DOWN;                 // 当某个键按下不动时，并不会生成SDL_Event。
        } else if (state == ActionState::RELEASED_THIS_FRAME) {
            state = ActionState::INACTIVE;
        }
    }

    // 2. 处理所有待处理的 SDL 事件 (这将设定 action_states_ 的值)
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        processEvent(event);
    }
}

void InputManager::processEvent(const SDL_Event& event) {
    switch (event.type) {
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP: {
            SDL_Scancode scancode = event.key.scancode;     // 获取按键的scancode
            bool is_down = event.key.down; 
            bool is_repeat = event.key.repeat;

            auto it = input_to_actions_map_.find(scancode);
            if (it != input_to_actions_map_.end()) {     // 如果按键有对应的action
                const std::vector<std::string>& associated_actions = it->second;
                for (const auto& action_name : associated_actions) {
                    updateActionState(action_name, is_down, is_repeat); // 更新action状态
                }
            }
            break;
        }
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP: {
            Uint32 button = event.button.button;              // 获取鼠标按钮
            bool is_down = event.button.down;
            auto it = input_to_actions_map_.find(button);
            if (it != input_to_actions_map_.end()) {     // 如果鼠标按钮有对应的action
                const std::vector<std::string>& associated_actions = it->second;
                for (const auto& action_name : associated_actions) {
                    // 鼠标事件不考虑repeat, 所以第三个参数传false
                    updateActionState(action_name, is_down, false); // 更新action状态
                }
            }
            // 在点击时更新鼠标位置
            mouse_position_ = {event.button.x, event.button.y};
            break;
        }
        case SDL_EVENT_MOUSE_MOTION:        // 处理鼠标运动
            mouse_position_ = {event.motion.x, event.motion.y};
            break;
        case SDL_EVENT_QUIT:
            should_quit_ = true;
            break;
        default:
            break;
    }
}

// --- 状态查询方法 ---

bool InputManager::isActionDown(std::string_view action_name) const {
    // C++17 引入的 “带有初始化语句的 if 语句”
    if (auto it = action_states_.find(std::string(action_name)); it != action_states_.end()) {
        return it->second == ActionState::PRESSED_THIS_FRAME || it->second == ActionState::HELD_DOWN;
    }
    return false;
}

bool InputManager::isActionPressed(std::string_view action_name) const {
    if (auto it = action_states_.find(std::string(action_name)); it != action_states_.end()) {
        return it->second == ActionState::PRESSED_THIS_FRAME;
    }
    return false;
}

bool InputManager::isActionReleased(std::string_view action_name) const {
    if (auto it = action_states_.find(std::string(action_name)); it != action_states_.end()) {
        return it->second == ActionState::RELEASED_THIS_FRAME;
    }
    return false;
}

bool InputManager::shouldQuit() const {
    return should_quit_;
}

void InputManager::setShouldQuit(bool should_quit)
{
    should_quit_ = should_quit;
}

glm::vec2 InputManager::getMousePosition() const
{
    return mouse_position_;
}

glm::vec2 InputManager::getLogicalMousePosition() const
{
    glm::vec2 logical_pos;
    // 通过窗口坐标获取渲染坐标（逻辑坐标）
    SDL_RenderCoordinatesFromWindow(sdl_renderer_, mouse_position_.x, mouse_position_.y, &logical_pos.x, &logical_pos.y);
    return logical_pos;
}

// --- 初始化输入映射 ---

void InputManager::initializeMappings(const engine::core::Config* config) {
    spdlog::trace("initialize input mappings...");
    if (!config) {
        spdlog::error("Config is null pointer");
        throw std::runtime_error("Config is null pointer");
    }
    actions_to_keyname_map_ = config->input_mappings_;      // 获取配置中的输入映射（动作 -> 按键名称）
    input_to_actions_map_.clear();
    action_states_.clear();

    // 如果配置中没有定义鼠标按钮动作(通常不需要配置),则添加默认映射, 用于 UI
    if (actions_to_keyname_map_.find("MouseLeftClick") == actions_to_keyname_map_.end()) {
         spdlog::debug("config does not define 'MouseLeftClick' action, add default mapping to 'MouseLeft'.");
         actions_to_keyname_map_["MouseLeftClick"] = {"MouseLeft"};     // 如果缺失则添加默认映射
    }
     if (actions_to_keyname_map_.find("MouseRightClick") == actions_to_keyname_map_.end()) {
         spdlog::debug("config does not define 'MouseRightClick' action, add default mapping to 'MouseRight'.");
         actions_to_keyname_map_["MouseRightClick"] = {"MouseRight"};   // 如果缺失则添加默认映射
    }
    // 遍历 动作 -> 按键名称 的映射
    for (const auto& [action_name, key_names] : actions_to_keyname_map_) {
        // 每个动作对应一个动作状态，初始化为 INACTIVE
        action_states_[action_name] = ActionState::INACTIVE;
        spdlog::trace("map action: {}", action_name);
        // 设置 "按键 -> 动作" 的映射
        for (const auto& key_name : key_names) {
            SDL_Scancode scancode = scancodeFromString(key_name);       // 尝试根据按键名称获取scancode
            Uint32 mouse_button = mouseButtonFromString(key_name);  // 尝试根据按键名称获取鼠标按钮
            // 未来可添加其它输入类型 ...

            if (scancode != SDL_SCANCODE_UNKNOWN) {      // 如果scancode有效,则将action添加到scancode_to_actions_map_中
                input_to_actions_map_[scancode].push_back(action_name);     
                spdlog::trace("  map key: {} (Scancode: {}) to action: {}", key_name, static_cast<int>(scancode), action_name);
            } else if (mouse_button != 0) {             // 如果鼠标按钮有效,则将action添加到mouse_button_to_actions_map_中
                input_to_actions_map_[mouse_button].push_back(action_name); 
                spdlog::trace("  map mouse button: {} (Button ID: {}) to action: {}", key_name, static_cast<int>(mouse_button), action_name);
                // else if: 未来可添加其它输入类型 ...
            } else {
                spdlog::warn("input mapping warning: unknown key or mouse button name '{}' for action '{}'.", key_name, action_name);
            }
        }
    }
    spdlog::trace("input mappings initialized.");
}

// --- 工具函数 ---
// 将字符串名称转换为 SDL_Scancode
SDL_Scancode InputManager::scancodeFromString(std::string_view key_name) {
    return SDL_GetScancodeFromName(key_name.data());
}

// 将鼠标按钮名称字符串转换为 SDL 按钮 Uint32 值
Uint32 InputManager::mouseButtonFromString(std::string_view button_name) {
    if (button_name == "MouseLeft") return SDL_BUTTON_LEFT;
    if (button_name == "MouseMiddle") return SDL_BUTTON_MIDDLE;
    if (button_name == "MouseRight") return SDL_BUTTON_RIGHT;
    // SDL 还定义了 SDL_BUTTON_X1 和 SDL_BUTTON_X2
    if (button_name == "MouseX1") return SDL_BUTTON_X1;
    if (button_name == "MouseX2") return SDL_BUTTON_X2;
    return 0; // 0 不是有效的按钮值，表示无效
}

void InputManager::updateActionState(std::string_view action_name, bool is_input_active, bool is_repeat_event) {
    auto it = action_states_.find(std::string(action_name));
    if (it == action_states_.end()) {
        spdlog::warn("try to update state of unregistered action: {}", action_name);
        return;
    }

    if (is_input_active) { // 输入被激活 (按下)
        if (is_repeat_event) {
            it->second = ActionState::HELD_DOWN; 
        } else {            // 非重复的按下事件
            it->second = ActionState::PRESSED_THIS_FRAME;
        }
    } else { // 输入被释放 (松开)
        it->second = ActionState::RELEASED_THIS_FRAME;
    }
}

} // namespace engine::input 