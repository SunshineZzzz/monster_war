#include "game_app.h"
#include "time.h"
#include "context.h"
#include "config.h"
#include "game_state.h"
#include "../resource/resource_manager.h"
#include "../audio/audio_player.h"
#include "../render/renderer.h"
#include "../render/camera.h"
#include "../render/text_renderer.h"
#include "../input/input_manager.h"
#include "../scene/scene_manager.h"
#include "../utils/events.h"
#include <SDL3/SDL.h>
#include <spdlog/spdlog.h>
#include <entt/signal/dispatcher.hpp>


namespace engine::core {

GameApp::GameApp() = default;

GameApp::~GameApp() {
    if (is_running_) {
        spdlog::warn("GameApp is running when destroyed.");
        close();
    }
}

void GameApp::run() {
    if (!init()) {
        spdlog::error("GameApp init failed, can not run game.");
        return;
    }

    while (is_running_) {
        time_->update();
        float delta_time = time_->getDeltaTime();
        
        handleEvents();
        update(delta_time);
        render();

        // spdlog::info("delta_time: {}", delta_time);
    }

    close();
}

void GameApp::registerSceneSetup(std::function<void(engine::core::Context&)> func)
{
    scene_setup_func_ = std::move(func);
    spdlog::trace("scene setup function registered.");
}

bool GameApp::init() {
    spdlog::trace("initializing GameApp ...");
    if (!scene_setup_func_) {
        spdlog::error("no scene setup function registered, can not initialize GameApp.");
        return false;
    }
    if (!initDispatcher()) return false;
    if (!initConfig()) return false;
    if (!initSDL())  return false;
    if (!initTime()) return false;
    if (!initResourceManager()) return false;
    if (!initAudioPlayer()) return false;
    if (!initRenderer()) return false;
    if (!initCamera()) return false;
    if (!initTextRenderer()) return false;
    if (!initInputManager()) return false;
    if (!initGameState()) return false;

    if (!initContext()) return false;
    if (!initSceneManager()) return false;

    // 调用场景设置函数 (创建第一个场景并压入栈)
    scene_setup_func_(*context_);

    // 注册退出事件 (回调函数可以无参数，代表不使用事件结构体中的数据)
    dispatcher_->sink<utils::QuitEvent>().connect<&GameApp::onQuitEvent>(this);

    is_running_ = true;
    spdlog::trace("GameApp initialized successfully.");
    return true;
}

void GameApp::handleEvents() {
    // 处理并分发输入事件
    input_manager_->update();

    scene_manager_->handleInput();
}

void GameApp::update(float delta_time) {
    // 游戏逻辑更新
    scene_manager_->update(delta_time);

    // 分发事件(分发消息队列中事件)
    dispatcher_->update();
}

void GameApp::render() {
    // 1. 清除屏幕
    renderer_->clearScreen();

    // 2. 具体渲染代码
    scene_manager_->render();

    // 3. 更新屏幕显示
    renderer_->present();
}

void GameApp::close() {
    spdlog::trace("closing GameApp ...");

    // 断开事件处理函数
    dispatcher_->sink<utils::QuitEvent>().disconnect<&GameApp::onQuitEvent>(this);

    // 先关闭场景管理器，确保所有场景都被清理
    scene_manager_->close();

    // 为了确保正确的销毁顺序，有些智能指针对象也需要手动管理
    resource_manager_.reset();

    if (sdl_renderer_ != nullptr) {
        SDL_DestroyRenderer(sdl_renderer_);
        sdl_renderer_ = nullptr;
    }
    if (window_ != nullptr) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    SDL_Quit();
    is_running_ = false;
}

bool GameApp::initDispatcher()
{
    try {
        dispatcher_ = std::make_unique<entt::dispatcher>();
    } catch (const std::exception& e) {
        spdlog::error("initialize event dispatcher failed: {}", e.what());
        return false;
    }
    spdlog::trace("event dispatcher initialized successfully.");
    return true;
}

bool GameApp::initConfig()
{
    try {
        config_ = std::make_unique<engine::core::Config>("assets/config.json");
    } catch (const std::exception& e) {
        spdlog::error("initialize config failed: {}", e.what());
        return false;
    }
    spdlog::trace("config initialized successfully.");
    return true;
}

bool GameApp::initSDL()
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        spdlog::error("SDL initialization failed! SDL error: {}", SDL_GetError());
        return false;
    }

    window_ = SDL_CreateWindow(config_->window_title_.c_str(), config_->window_width_, config_->window_height_, SDL_WINDOW_RESIZABLE);
    if (window_ == nullptr) {
        spdlog::error("SDL window creation failed! SDL error: {}", SDL_GetError());
        return false;
    }

    sdl_renderer_ = SDL_CreateRenderer(window_, nullptr);
    if (sdl_renderer_ == nullptr) {
        spdlog::error("SDL renderer creation failed! SDL error: {}", SDL_GetError());
        return false;
    }

    // 设置渲染器支持透明色
    SDL_SetRenderDrawBlendMode(sdl_renderer_, SDL_BLENDMODE_BLEND);

    // 设置 VSync (注意: VSync 开启时，驱动程序会尝试将帧率限制到显示器刷新率，有可能会覆盖我们手动设置的 target_fps)
    int vsync_mode = config_->vsync_enabled_ ? SDL_RENDERER_VSYNC_ADAPTIVE : SDL_RENDERER_VSYNC_DISABLED;
    if (!SDL_SetRenderVSync(sdl_renderer_, vsync_mode)) {
        spdlog::warn("SDL_SetRenderVSync failed! SDL error: {}", SDL_GetError());
    }
    spdlog::trace("VSync set: {}", config_->vsync_enabled_ ? "Enabled" : "Disabled");

    // 设置逻辑分辨率为窗口大小的一半（针对像素游戏）
    SDL_SetRenderLogicalPresentation(sdl_renderer_, config_->window_width_ / 2, config_->window_height_ / 2, SDL_LOGICAL_PRESENTATION_LETTERBOX);
    spdlog::trace("SDL initialized successfully.");
    return true;
}

bool GameApp::initTime() {
    try {
        time_ = std::make_unique<Time>();
    } catch (const std::exception& e) {
        spdlog::error("initialize time manager failed: {}", e.what());
        return false;
    }
    time_->setTargetFps(config_->target_fps_);
    spdlog::trace("time manager initialized successfully.");
    return true;
}

bool GameApp::initResourceManager() {
    try {
        resource_manager_ = std::make_unique<engine::resource::ResourceManager>(sdl_renderer_);
    } catch (const std::exception& e) {
        spdlog::error("initialize resource manager failed: {}", e.what());
        return false;
    }
    spdlog::trace("resource manager initialized successfully.");
    return true;
}

bool GameApp::initAudioPlayer()
{
    try {
        audio_player_ = std::make_unique<engine::audio::AudioPlayer>(resource_manager_.get());
        audio_player_->setMusicVolume(config_->music_volume_);      // 设置背景音乐音量
        audio_player_->setSoundVolume(config_->sound_volume_);      // 设置音效音量
    } catch (const std::exception& e) {
        spdlog::error("initialize audio player failed: {}", e.what());
        return false;
    }
    spdlog::trace("audio player initialized successfully.");
    return true;
}

bool GameApp::initRenderer() {
    try {
        renderer_ = std::make_unique<engine::render::Renderer>(sdl_renderer_, resource_manager_.get());
    } catch (const std::exception& e) {
        spdlog::error("initialize renderer failed: {}", e.what());
        return false;
    }
    spdlog::trace("renderer initialized successfully.");
    return true;
}

bool GameApp::initCamera() {
    try {
        camera_ = std::make_unique<engine::render::Camera>(glm::vec2(config_->window_width_ / 2, config_->window_height_ / 2));
    } catch (const std::exception& e) {
        spdlog::error("initialize camera failed: {}", e.what());
        return false;
    }
    spdlog::trace("camera initialized successfully.");
    return true;
}

bool GameApp::initTextRenderer()
{
    try {
        text_renderer_ = std::make_unique<engine::render::TextRenderer>(sdl_renderer_, resource_manager_.get());
    } catch (const std::exception& e) {
        spdlog::error("initialize text renderer failed: {}", e.what());
        return false;
    }
    spdlog::trace("text renderer initialized successfully.");
    return true;
}

bool GameApp::initInputManager()
{
    try {
        input_manager_ = std::make_unique<engine::input::InputManager>(sdl_renderer_, config_.get(), dispatcher_.get());
    } catch (const std::exception& e) {
        spdlog::error("initialize input manager failed: {}", e.what());
        return false;
    }
    spdlog::trace("input manager initialized successfully.");
    return true;
}

bool GameApp::initGameState()
{
    try {
        game_state_ = std::make_unique<engine::core::GameState>(window_, sdl_renderer_);
    } catch (const std::exception& e) {
        spdlog::error("initialize game state failed: {}", e.what());
        return false;
    }
    spdlog::trace("game state initialized successfully.");
    resource_manager_->loadResources("assets/data/resource_mapping.json");  // 载入默认资源映射文件
    return true;
}

bool GameApp::initContext()
{
    try {
        context_ = std::make_unique<engine::core::Context>(*dispatcher_,
                                                           *input_manager_,
                                                           *renderer_, 
                                                           *camera_, 
                                                           *text_renderer_,
                                                           *resource_manager_, 
                                                           *audio_player_,
                                                           *game_state_);
    } catch (const std::exception& e) {
        spdlog::error("initialize context failed: {}", e.what());
        return false;
    }
    spdlog::trace("context initialized successfully.");
    return true;
}

bool GameApp::initSceneManager()
{
    try {
        scene_manager_ = std::make_unique<engine::scene::SceneManager>(*context_);
    } catch (const std::exception& e) {
        spdlog::error("initialize scene manager failed: {}", e.what());
        return false;
    }
    spdlog::trace("scene manager initialized successfully.");
    return true;
}

void GameApp::onQuitEvent()
{
    spdlog::trace("GameApp received quit event from event dispatcher.");
    is_running_ = false;
}

} // namespace engine::core