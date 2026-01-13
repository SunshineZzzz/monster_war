#include "game_scene.h"
#include "../../engine/core/context.h"
#include "../../engine/input/input_manager.h"
#include "../../engine/utils/events.h"
#include <entt/signal/sigh.hpp>
#include <entt/signal/dispatcher.hpp>
#include <spdlog/spdlog.h>

namespace game::scene {

GameScene::GameScene(engine::core::Context& context)
    : Scene("GameScene", context) {
}

GameScene::~GameScene() {
}

void GameScene::init() {
    // 测试场景编号, 每创建一个场景, 编号加1
    static int count = 0;
    scene_num_ = count++;
    spdlog::info("game scene number: {}", scene_num_);

    // 注册输入回调事件
    auto& input_manager = context_.getInputManager();
    input_manager.onAction("jump").connect<&GameScene::onReplace>(this);       // J 键
    input_manager.onAction("mouse_left").connect<&GameScene::onPush>(this);   // 鼠标左键
    input_manager.onAction("mouse_right").connect<&GameScene::onPop>(this);   // 鼠标右键
    input_manager.onAction("pause").connect<&GameScene::onQuit>(this);        // P 键

    Scene::init();
}

void GameScene::clean() {
    // 断开输入回调事件 (谁连接，谁负责断开)
    auto& input_manager = context_.getInputManager();
    input_manager.onAction("jump").disconnect<&GameScene::onReplace>(this);
    input_manager.onAction("mouse_left").disconnect<&GameScene::onPush>(this);
    input_manager.onAction("mouse_right").disconnect<&GameScene::onPop>(this);
    input_manager.onAction("pause").disconnect<&GameScene::onQuit>(this);

    Scene::clean();
}

bool GameScene::onReplace() {
    spdlog::info("onReplace, switch to new game scene");
    requestReplaceScene(std::make_unique<game::scene::GameScene>(context_));
    return true;
}

bool GameScene::onPush() {
    spdlog::info("onPush, push new game scene");
    requestPushScene(std::make_unique<game::scene::GameScene>(context_));
    return true;
}

bool GameScene::onPop() {
    spdlog::info("onPop, pop scene number: {}", scene_num_);
    requestPopScene();
    return true;
}

bool GameScene::onQuit() {
    spdlog::info("onQuit, exit game");
    quit();
    return true;
}

} // namespace game::scene
