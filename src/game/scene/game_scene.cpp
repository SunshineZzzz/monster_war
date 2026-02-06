#include "game_scene.h"
#include "title_scene.h"
#include "level_clear_scene.h"
#include "end_scene.h"
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
#include "../system/debug_ui_system.h"
#include "../system/selection_system.h"
#include "../system/skill_system.h"
#include "../ui/units_portrait_ui.h"
#include "../../engine/audio/audio_player.h"
#include "../../engine/core/context.h"
#include "../../engine/core/game_state.h"
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

GameScene::GameScene(engine::core::Context& context,
    std::shared_ptr<game::factory::BlueprintManager> blueprint_manager,
    std::shared_ptr<game::data::SessionData> session_data,
    std::shared_ptr<game::data::UIConfig> ui_config,
    std::shared_ptr<game::data::LevelConfig> level_config)
    : engine::scene::Scene("GameScene", context),
      blueprint_manager_(blueprint_manager),
      session_data_(session_data),
      ui_config_(ui_config),
      level_config_(level_config)
{
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

    context_.getGameState().setState(engine::core::State::Playing);
    context_.getAudioPlayer().playMusic("battle_bgm"_hs);
    Scene::init();
}

void GameScene::update(float delta_time) {
    auto& dispatcher = context_.getDispatcher();

    // 事件总线处理完一下内容
    // 引擎层动画系统，接收切换动画事件;
    // 引擎层音频系统，接受播放音效事件;
    // 游戏层动画状态系统，处理动画播放完毕，根据状态发送切换动画事件，如果是一次性动画实体，添加死亡标签;
    // 游戏层动画事件系统，处理动画事件，发送动画事件(攻击事件，治疗事件，发射投射物事件等)，各发送音效事件到事件总线;
    // 游戏层战斗结算系统，处理攻击事件，治疗事件，修改实体状态(血量等)，发送玩家移除单位事件，敌人添加死亡标签，发送特效事件到事件总线等;
    // 游戏层面投射物系统，处理投射物事件，创建投射物实体
    // 游戏层面特效系统，处理特效事件，创建特效实体
    // 游戏层游戏规则系统，处理敌人到达基地事件，处理玩家升级事件，加入特效和音频事件，处理玩家撤退事件，移除单位事件;
    // 游戏层放置单位系统，处理准备放置单位事件，移除(地图上)玩家单位事件，处理鼠标点击尝试将准备放置单位放置到地图上事件，
    //      如果拥有被动技能，则添加技能激活事件，处理鼠标右键尝试将准备放置单位移除事件;
    // 处理玩家移除单位事件，标记该单位为死亡，移除占用组件;
    // 游戏层选择系统，处理鼠标左键在玩家单位上的点击事件，从而显示玩家单位的属性；右键取消显示
    // 游戏层技能系统，处理技能准备就绪事件，创建技能显示实体，如果是存在改变玩家动画，并且玩家没有锁定状态，则改变玩家动画; 
    //      处理技能激活事件，移除技能准备标签，添加技能激活标签，创建技能显示实体，添加BUFF;
    //      处理技能持续结束事件，移除技能激活标签，移除BUFF，如果是存在改变玩家动画，并且玩家没有锁定状态，则改变玩家动画;
    //      处理玩家移除单位事件，移除技能显示实体。

    // 每一帧最先清理死亡实体(要在dispatcher处理完事件后再清理，因此放在下一帧开头)
    remove_dead_system_->update(registry_);

    // 暂停状态下，有些功能依然正常运行
    if (context_.getGameState().isPaused()) {
        place_unit_system_->update(delta_time);
        ysort_system_->update(registry_);
        selection_system_->update();
        units_portrait_ui_->update(delta_time);
        Scene::update(delta_time);
        return;
    }

    // 注意系统更新的顺序
    // 冷却时间到了加上可攻击标签(AttackReadyTag);
    // 技能冷却到了加上可释放标签(SkillReadyTag)，发送技能准备就绪事件，等待UI系统触发添加技能激活事件;
    // 技能持续时间到了移除技能激活标签(SkillActiveTag)，发送技能持续结束事件;
    timer_system_->update(delta_time);
    // 更新当前场景的cost
    game_rule_system_->update(delta_time);
    // 敌人如果被阻挡，添加阻挡组件(BlockedByComponent);
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
    // 处理鼠标在玩家单位上或者敌人单位上的悬停事件
    selection_system_->update();

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
    // 当场景栈中只有GameScene时才渲染调试UI, 不然上层有其它场景时会冲突
    if (context_.getGameState().isPlaying() || context_.getGameState().isPaused()) {
        debug_ui_system_->update();     // 调试UI的显示优先级最高，最后渲染
    }
}

void GameScene::clean() {
    auto& dispatcher = context_.getDispatcher();
    // 断开所有事件连接
    dispatcher.disconnect(this);
    // 断开输入信号连接
    Scene::clean();
}

bool GameScene::initSessionData() {
    if (!session_data_) {
        session_data_ = std::make_shared<game::data::SessionData>();
        if (!session_data_->loadDefaultData()) {
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
        return false;
    }
    return true;
}

bool GameScene::initEventConnections() {
    auto& dispatcher = context_.getDispatcher();
    dispatcher.sink<game::defs::RestartEvent>().connect<&GameScene::onRestart>(this);
    dispatcher.sink<game::defs::BackToTitleEvent>().connect<&GameScene::onBackToTitle>(this);
    dispatcher.sink<game::defs::SaveEvent>().connect<&GameScene::onSave>(this);
    dispatcher.sink<game::defs::LevelClearEvent>().connect<&GameScene::onLevelClear>(this);
    dispatcher.sink<game::defs::GameEndEvent>().connect<&GameScene::onGameEndEvent>(this);
    return true;
}

bool GameScene::initInputConnections() {
    // 未来可添加输入控制，记得在close函数中断开
    return true;
}

bool GameScene::initEntityFactory() {
    // 如果蓝图管理器为空，则创建一个（将来可能由构造函数传入）
    if (!blueprint_manager_) {  
        blueprint_manager_ = std::make_shared<game::factory::BlueprintManager>(context_.getResourceManager());
        if (!blueprint_manager_->loadEnemyClassBlueprints("assets/data/enemy_data.json") ||
            !blueprint_manager_->loadPlayerClassBlueprints("assets/data/player_data.json") ||
            !blueprint_manager_->loadProjectileBlueprints("assets/data/projectile_data.json") ||
            !blueprint_manager_->loadEffectBlueprints("assets/data/effect_data.json") ||
            !blueprint_manager_->loadSkillBlueprints("assets/data/skill_data.json")) {
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
    registry_.ctx().emplace_as<entt::entity&>("selected_unit"_hs, selected_unit_);
    registry_.ctx().emplace_as<entt::entity&>("hovered_unit"_hs, hovered_unit_);
    registry_.ctx().emplace_as<bool&>("show_save_panel"_hs, show_save_panel_);
    spdlog::info("registry_ context init complete");
    return true;
}

bool GameScene::initUnitsPortraitUI() {
    try {
        units_portrait_ui_ = std::make_unique<game::ui::UnitsPortraitUI>(registry_, *ui_manager_, context_);
    } catch (const std::exception& e) {
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
    timer_system_ = std::make_unique<game::system::TimerSystem>(registry_, dispatcher);
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
    debug_ui_system_ = std::make_unique<game::system::DebugUISystem>(registry_, context_);
    selection_system_ = std::make_unique<game::system::SelectionSystem>(registry_, context_);
    skill_system_ = std::make_unique<game::system::SkillSystem>(registry_, dispatcher, *entity_factory_);
    spdlog::info("system init complete");
    return true;
}

bool GameScene::initEnemySpawner() {
    enemy_spawner_ = std::make_unique<game::spawner::EnemySpawner>(registry_, *entity_factory_);
    spdlog::info("enemy_spawner_ init complete");
    return true;
}

// --- 场景相关函数 ---
void GameScene::onRestart() {
    spdlog::info("restart level");
    requestReplaceScene(std::make_unique<game::scene::GameScene>(
        context_, 
        blueprint_manager_,
        session_data_,
        ui_config_,
        level_config_
        )
    );
}

void GameScene::onBackToTitle() {
    spdlog::info("返回标题");
    requestReplaceScene(std::make_unique<game::scene::TitleScene>(context_));
}

void GameScene::onSave() {
    spdlog::info("保存");
    show_save_panel_ = !show_save_panel_;
    /* 用ImGui快速实现逻辑，未来再完善游戏内UI */
}

void GameScene::onLevelClear() {
    spdlog::info("level clear success");
    // 奖励点数 = 击杀数 + 基地血量 * 5
    const auto point = game_stats_.enemy_killed_count_ + game_stats_.home_hp_ * 5;
    session_data_->setLevelClear(true);
    session_data_->addPoint(point);

    // 如果当前关卡是最后一关，则进入结束场景；否则进入通关结算场景
    if (level_config_->isFinalLevel(level_number_)) {
        requestPushScene(std::make_unique<game::scene::EndScene>(context_, true));
    } else {
        requestPushScene(std::make_unique<game::scene::LevelClearScene>(
        context_,
        blueprint_manager_,
        ui_config_,
        level_config_,
        session_data_,
        game_stats_
        ));
    }
}

void GameScene::onGameEndEvent(const game::defs::GameEndEvent& event) {
    spdlog::info("game end, is_win: {}", event.is_win_);
    requestPushScene(std::make_unique<game::scene::EndScene>(context_, event.is_win_));
}

} // namespace game::scene
