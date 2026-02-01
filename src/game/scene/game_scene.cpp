#include "game_scene.h"
#include "../component/player_component.h"
#include "../component/stats_component.h"
#include "../factory/entity_factory.h"
#include "../factory/blueprint_manager.h"
#include "../loader/entity_builder_mw.h"
#include "../spawner/enemy_spawner.h"
#include "../system/followpath_system.h"
#include "../system/remove_dead_system.h"
#include "../system/block_system.h"
#include "../system/set_target_system.h"
#include "../system/attack_starter_system.h"
#include "../system/timer_system.h"
#include "../system/orientation_system.h"
#include "../system/animation_state_system.h"
#include "../system/animation_event_system.h"
#include "../system/combat_resolve_system.h"
#include "../system/projectile_system.h"
#include "../system/effect_system.h"
#include "../system/health_bar_system.h"
#include "../system/game_rule_system.h"
#include "../system/place_unit_system.h"
#include "../system/render_range_system.h"
#include "../ui/units_portrait_ui.h"
#include "../defs/tags.h"
#include "../../engine/input/input_manager.h"
#include "../../engine/core/context.h"
#include "../../engine/core/game_state.h"
#include "../../engine/render/camera.h"
#include "../../engine/system/render_system.h"
#include "../../engine/system/movement_system.h"
#include "../../engine/system/animation_system.h"
#include "../../engine/system/ysort_system.h"
#include "../../engine/system/audio_system.h"
#include "../../engine/loader/level_loader.h"
#include "../../engine/ui/ui_manager.h"
#include <entt/core/hashed_string.hpp>
#include <entt/signal/sigh.hpp>
#include <spdlog/spdlog.h>

using namespace entt::literals;

namespace game::scene {

GameScene::GameScene(engine::core::Context& context)
    : Scene("GameScene", context) {
    spdlog::info("GameScene build complete");
}

GameScene::~GameScene() {
}

void GameScene::init() {
    if (!initSessionData()) {
        spdlog::error("init session_data_ failed");
        return;
    }
    if (!initLevelConfig()) { 
        spdlog::error("init level config failed"); 
        return; 
    }
    if (!initUIConfig()) {
        spdlog::error("init ui_config_ failed");
        return;
    }
    if (!loadLevel()) {
        spdlog::error("load level failed");
        return;
    }
    if (!initEventConnections()) {
        spdlog::error("init event connections failed");
        return;
    }
    if (!initInputConnections()) {
        spdlog::error("init input connections failed");
        return;
    }
    if (!initEntityFactory()) {
        spdlog::error("init entity factory failed");
        return;
    }
    if (!initRegistryContext()) { 
        spdlog::error("init registry context failed"); 
        return; 
    }
    if (!initUnitsPortraitUI()) { 
        spdlog::error("init units portrait ui failed"); 
        return; 
    }
    if (!initSystems()) {
        spdlog::error("init systems failed");
        return;
    }
    if (!initEnemySpawner()) { 
        spdlog::error("init enemy spawner failed"); 
        return; 
    }

    Scene::init();
}

void GameScene::update(float delta_time) {
    auto& dispatcher = context_.getDispatcher();

    // 事件总线处理完一下内容
    // 引擎层动画系统，接收切换动画事件;
    // 引擎层音频系统，接受播放音效事件;
    // 游戏层动画状态系统，处理动画播放完毕，根据状态发送切换动画事件，如果是一次性动画实体，添加死亡标签;
    // 游戏层动画事件系统，处理动画事件，发送动画事件(攻击事件，治疗事件，发射投射物事件等)，各发送音效事件到事件总线;
    // 游戏层战斗结算系统，处理攻击事件，治疗事件，修改实体状态(血量等)，添加死亡标签，发送特效事件到事件总线等;
    // 游戏层面投射物系统，处理投射物事件，创建投射物实体
    // 游戏层面特效系统，处理特效事件，创建特效实体
    // 游戏层游戏规则系统，处理敌人到达基地事件
    // 游戏层放置单位系统，处理准备放置单位事件，移除(地图上)玩家单位事件，处理鼠标点击尝试将准备放置单位放置到地图上事件，处理鼠标右键尝试将准备放置单位移除事件

    // 每一帧最先清理死亡实体(要在dispatcher处理完事件后再清理，因此放在下一帧开头)
    remove_dead_system_->update(registry_);

    // 注意系统更新的顺序
    // 冷却时间到了加上可攻击标签(AttackReadyTag);
    timer_system_->update(registry_, delta_time);
    // 更新当前场景的cost
    game_rule_system_->update(delta_time);
    // 敌人如果被阻挡，添加阻挡组件(BlockedByComponent);
    // 事件总线加入敌人攻击动画事件;
    block_system_->update(registry_, dispatcher);
    // 有目标敌人或者玩家判断是否有效，无效，删除目标组件(TargetComponent);
    // 玩家攻击性角色设置目标组件(TargetComponent);
    // 远程敌人角色设置目标组件(TargetComponent);
    // 玩家治疗者角色设置目标组件(TargetComponent);
    set_target_system_->update(registry_);
    // 排除“被阻挡的敌人”和“动作锁定敌人”，根据下一个目标节点计算速度向量
    follow_path_system_->update(registry_, dispatcher, waypoint_nodes_);
    // 解决敌我双方朝向问题
    orientation_system_->update(registry_);     // 调用顺序要在Block、SetTarget、FollowPath之后
    // 被阻挡的敌人，攻击冷却完毕，移除可攻击标签(AttackReadyTag)，事件总线加入可攻击动画事件;
    // 有目标的远程敌人，未被阻挡，攻击冷却完毕，移除可攻击标签(AttackReadyTag)，速度向量设为0，事件总线加入可攻击动画事件;
    // 有目标的玩家，攻击冷却完毕，移除可攻击标签(AttackReadyTag)，事件总线加入可攻击动画事件;
    attack_starter_system_->update(registry_, dispatcher);
    // 更新投射物状态，投射物到达目标位置，发送攻击事件和播放音效到事件总线
    projectile_system_->update(delta_time);
    // 移动
    movement_system_->update(registry_, delta_time);
    // 有动画事件(比如:攻击事件)加入事件总线，有动画完毕事件加入事件总线
    animation_system_->update(delta_time);
    // 准备放置单位在世界移动颜色变化和鼠标跟随
    place_unit_system_->update(delta_time);
    // 让RenderComponent的深度depth等于TransformComponent的y坐标
    ysort_system_->update(registry_);   // 调用顺序要在MovementSystem之后

    // 场景中其他更新函数
    enemy_spawner_->update(delta_time);
    // 场景中头像UI更新
    units_portrait_ui_->update(delta_time);
    // UI更新等
    Scene::update(delta_time);
}

void GameScene::render() {
    auto& renderer = context_.getRenderer();
    auto& camera = context_.getCamera();
    
    // 注意渲染顺序，保证正确的遮盖关系
    render_system_->update(registry_, renderer, camera);
    health_bar_system_->update(registry_, renderer, camera);
    render_range_system_->update(registry_, renderer, camera);

    Scene::render();
}

void GameScene::clean() {
    auto& dispatcher = context_.getDispatcher();
    auto& input_manager = context_.getInputManager();
    // 断开所有事件连接
    dispatcher.disconnect(this);
    // 断开输入信号连接
    input_manager.onAction("pause"_hs).disconnect<&GameScene::onClearAllPlayers>(this);
    Scene::clean();
}

bool GameScene::initSessionData() {
    if (!session_data_) {
        session_data_ = std::make_shared<game::data::SessionData>();
        if (!session_data_->loadDefaultData()) {
            spdlog::error("init session_data_ failed");
            return false;
        }
    }
    level_number_ = session_data_->getLevelNumber();
    return true;
}

bool GameScene::initLevelConfig() {
    if (!level_config_) {
        level_config_ = std::make_shared<game::data::LevelConfig>();
        if (!level_config_->loadFromFile("assets/data/level_config.json")) {
            spdlog::error("init level_config_ failed");
            return false;
        }
    }
    waves_ = level_config_->getWavesData(level_number_);
    game_stats_.enemy_count_ = level_config_->getTotalEnemyCount(level_number_);
    return true;
}

bool GameScene::initUIConfig() {
    if (!ui_config_) {
        ui_config_ = std::make_shared<game::data::UIConfig>();
        if (!ui_config_->loadFromFile("assets/data/ui_config.json")) {
            spdlog::error("load ui_config_ failed");
            return false;
        }
    }
    return true;
}

bool GameScene::loadLevel() {
    engine::loader::LevelLoader level_loader;
    // 设置拓展的构建器EntityBuilderMW
    level_loader.setEntityBuilder(std::make_unique<game::loader::EntityBuilderMW>(level_loader, 
        context_, 
        registry_, 
        waypoint_nodes_, 
        start_points_)
    );
    // 获取关卡地图路径
    auto map_path = level_config_->getMapPath(level_number_);
    if (!level_loader.loadLevel(map_path, this)) {
        spdlog::error("load level failed");
        return false;
    }
    return true;
}

bool GameScene::initEventConnections() {
    // auto& dispatcher = context_.getDispatcher();
    return true;
}

bool GameScene::initInputConnections() {
    auto& input_manager = context_.getInputManager();
    input_manager.onAction("pause"_hs).connect<&GameScene::onClearAllPlayers>(this);
    return true;
}

bool GameScene::initEntityFactory() {
    // 如果蓝图管理器为空，则创建一个（将来可能由构造函数传入）
    if (!blueprint_manager_) {  
        blueprint_manager_ = std::make_shared<game::factory::BlueprintManager>(context_.getResourceManager());
        if (!blueprint_manager_->loadEnemyClassBlueprints("assets/data/enemy_data.json") || 
            !blueprint_manager_->loadPlayerClassBlueprints("assets/data/player_data.json") ||
            !blueprint_manager_->loadProjectileBlueprints("assets/data/projectile_data.json")) {
            spdlog::error("load blueprints failed");
            return false;
        }
    }
    entity_factory_ = std::make_unique<game::factory::EntityFactory>(registry_, *blueprint_manager_);
    spdlog::info("entity_factory_ init complete");
    return true;
}

bool GameScene::initRegistryContext() {
    // 让注册表存储一些数据类型实例作为上下文，方便使用
    registry_.ctx().emplace<std::shared_ptr<game::factory::BlueprintManager>>(blueprint_manager_);
    registry_.ctx().emplace<std::shared_ptr<game::data::SessionData>>(session_data_);
    registry_.ctx().emplace<std::shared_ptr<game::data::UIConfig>>(ui_config_);
    registry_.ctx().emplace<std::shared_ptr<game::data::LevelConfig>>(level_config_);
    registry_.ctx().emplace<std::unordered_map<int, game::data::WaypointNode>&>(waypoint_nodes_);
    registry_.ctx().emplace<std::vector<int>&>(start_points_);
    registry_.ctx().emplace<game::data::GameStats&>(game_stats_);
    registry_.ctx().emplace<game::data::Waves&>(waves_);
    registry_.ctx().emplace<int&>(level_number_);
    spdlog::info("registry_ context init complete");
    return true;
}

bool GameScene::initUnitsPortraitUI() {
    try {
        units_portrait_ui_ = std::make_unique<game::ui::UnitsPortraitUI>(registry_, *ui_manager_, context_);
    } catch (const std::exception& e) {
        spdlog::error("init units_portrait_ui_ failed: {}", e.what());
        return false;
    }
    return true;
}

bool GameScene::initSystems() {
    auto& dispatcher = context_.getDispatcher();
    // 系统初始化需要在可能的依赖模块(如实体工厂)初始化之后
    render_system_ = std::make_unique<engine::system::RenderSystem>();
    movement_system_ = std::make_unique<engine::system::MovementSystem>();
    animation_system_ = std::make_unique<engine::system::AnimationSystem>(registry_, dispatcher);
    ysort_system_ = std::make_unique<engine::system::YSortSystem>();
    audio_system_ = std::make_unique<engine::system::AudioSystem>(registry_, context_);

    follow_path_system_ = std::make_unique<game::system::FollowPathSystem>();
    remove_dead_system_ = std::make_unique<game::system::RemoveDeadSystem>();
    block_system_ = std::make_unique<game::system::BlockSystem>();
    set_target_system_ = std::make_unique<game::system::SetTargetSystem>();
    attack_starter_system_ = std::make_unique<game::system::AttackStarterSystem>();
    timer_system_ = std::make_unique<game::system::TimerSystem>();
    orientation_system_ = std::make_unique<game::system::OrientationSystem>();
    animation_state_system_ = std::make_unique<game::system::AnimationStateSystem>(registry_, dispatcher);
    animation_event_system_ = std::make_unique<game::system::AnimationEventSystem>(registry_, dispatcher);
    combat_resolve_system_ = std::make_unique<game::system::CombatResolveSystem>(registry_, dispatcher);
    projectile_system_ = std::make_unique<game::system::ProjectileSystem>(registry_, dispatcher, *entity_factory_);
    effect_system_ = std::make_unique<game::system::EffectSystem>(registry_, dispatcher, *entity_factory_);
    health_bar_system_ = std::make_unique<game::system::HealthBarSystem>();
    game_rule_system_ = std::make_unique<game::system::GameRuleSystem>(registry_, dispatcher);
    place_unit_system_ = std::make_unique<game::system::PlaceUnitSystem>(registry_, *entity_factory_, context_);
    render_range_system_ = std::make_unique<game::system::RenderRangeSystem>();
    spdlog::info("system init complete");
    return true;
}

bool GameScene::initEnemySpawner() {
    enemy_spawner_ = std::make_unique<game::spawner::EnemySpawner>(registry_, *entity_factory_);
    spdlog::info("enemy_spawner_ init complete");
    return true;
}

bool GameScene::onClearAllPlayers() {
    auto view = registry_.view<game::component::PlayerComponent>();
    for (auto entity : view) {
        context_.getDispatcher().enqueue(game::defs::RemovePlayerUnitEvent{entity});
    }
    return true;
}

} // namespace game::scene
