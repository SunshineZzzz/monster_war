#pragma once
#include <memory>

namespace engine::core {
    class Context;
}

namespace engine::ui {
    class UIInteractive;
}

namespace engine::ui::state {

/**
 * @brief 可交互UI元素在特定状态下的行为接口。
 *
 * 该接口定义了所有具体UI状态必须实现的通用操作，
 * 例如处理输入事件、更新状态逻辑以及确定视觉表现。
 * 
 * 状态模式
 * 状态模式的精髓在于，它允许我们将对象的行为封装在不同的状态类中，
 * 并根据对象的当前状态来调用不同的行为方法。
 * 这使得对象在不同状态下的行为可以动态改变，而无需修改对象的代码。
 * 
 * 使用状态模式也复合开闭原则，当需要添加新功能时，我们应该通过增加新代码来实现，而不是修改旧代码。
 */
class UIState {
    friend class engine::ui::UIInteractive;
protected:
    engine::ui::UIInteractive* owner_ = nullptr;    ///< @brief 指向父节点

public:
    /**
     * @brief 构造函数传入父节点指针
     */
    UIState(engine::ui::UIInteractive* owner) : owner_(owner) {}
    virtual ~UIState() = default;

    // 删除拷贝和移动构造函数/赋值运算符
    UIState(const UIState&) = delete;
    UIState& operator=(const UIState&) = delete;
    UIState(UIState&&) = delete;
    UIState& operator=(UIState&&) = delete;

protected:
    // --- 核心方法 --- 
    virtual void enter() {}
    virtual std::unique_ptr<UIState> handleInput(engine::core::Context& context) = 0;
};

} // namespace engine::ui::state