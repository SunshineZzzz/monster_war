#include "game_state.h"
#include <spdlog/spdlog.h>
#include <stdexcept>

namespace engine::core {

GameState::GameState(SDL_Window* window, SDL_Renderer* renderer, State initial_state)
    : window_(window), renderer_(renderer), current_state_(initial_state){
    if (window_ == nullptr || renderer_ == nullptr) {
        spdlog::error("window or renderer is nullptr");
        throw std::runtime_error("window or renderer is nullptr");
    }
    spdlog::trace("game state initialized");
}

void GameState::setState(State new_state) {
    if (current_state_ != new_state) {
        spdlog::debug("game state changed from {} to {}", static_cast<int>(current_state_), static_cast<int>(new_state));
        current_state_ = std::move(new_state);
    } else {
        spdlog::debug("try to set same game state, skip");
    }
}

glm::vec2 GameState::getWindowSize() const
{
    int width, height;
    // SDL3获取窗口大小的方法
    SDL_GetWindowSize(window_, &width, &height);
    return glm::vec2(width, height);
}

void GameState::setWindowSize(const glm::vec2& window_size)
{
    SDL_SetWindowSize(window_, static_cast<int>(window_size.x), static_cast<int>(window_size.y));
}

glm::vec2 GameState::getLogicalSize() const
{
    int width, height;
    // SDL3获取逻辑分辨率的方法
    SDL_GetRenderLogicalPresentation(renderer_, &width, &height, NULL);
    return glm::vec2(width, height);
}


void GameState::setLogicalSize(const glm::vec2& logical_size)
{
    SDL_SetRenderLogicalPresentation(renderer_, 
                                     static_cast<int>(logical_size.x), 
                                     static_cast<int>(logical_size.y),
                                     SDL_LOGICAL_PRESENTATION_LETTERBOX);
    spdlog::trace("logical size set to: {}x{}", logical_size.x, logical_size.y);
}


} // namespace engine::core 