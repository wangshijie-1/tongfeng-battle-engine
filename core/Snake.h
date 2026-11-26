#ifndef SNAKE_H
#define SNAKE_H


#include "World_Unit.h"
#include <deque>

// 朝向判断：UP↔DOWN、LEFT↔RIGHT 互为相反（基于 Direction 枚举值 0,1,2,3 的异或配对，
// 复杂度 O(1) 且无分支链；NONE 守卫避免把 NONE 误判为某方向的反方向）。
// 内联：move() 每帧调用，消除函数调用开销。
inline bool isOppositeDirection(Direction current, Direction next)
{
    return current != Direction::NONE && next != Direction::NONE &&
        (static_cast<int>(current) ^ 1) == static_cast<int>(next);
}

class Snake
{
private:
    std::deque<Position> body;      // 蛇的身体， 头在前，尾在后
    Direction currentDirection;     // 蛇的当前方向
    Direction nextDirection;        // 蛇的下一个方向
    bool growing;                   // 蛇是否在成长中
    bool dead;                      // 蛇是否死亡

    Position new_head;              // 蛇的新头部位置
    Position old_tail;              // 蛇的旧尾部位置
    int speed;                      // 蛇的速度
    int stamina;                    // 蛇的体力
    bool canBoost = true;           // 是否允许加速：体力耗尽强制解除加速后置 false，
                                    // 恢复体力 > 10 点后重新置 true（恢复逻辑在 move() 中维护）
    int headColor = 0xFF0000;       // 蛇头颜色（双人区分）
    int bodyColor = 0x00D4AA;       // 蛇身颜色（双人区分）

public:
    // 体力上限：加速每走一格 -1、正常每走一格 +1，值域 [0, MAX_STAMINA]
    // public：供外部（侧栏渲染等）以 Snake::MAX_STAMINA 访问
    static const int MAX_STAMINA = 100;

    Snake();                       // 默认构造
    Snake(Position head, Position tail);
    ~Snake();

    void move();                    // 移动蛇头， 更新蛇的身体和长度
    void grow();                    // 设置生长标志位
    void changeDirection(Direction dir);    // 更新下一个方向
    void setDirection(Direction dir);       // 直接设置当前+下一方向(用于开局/重置)
    // 在指定位置、朝向、长度初始化蛇身（双人模式对角出生用）。
    // 纯逻辑：只设置 body/方向/速度/体力，不触碰 Board（保持无 board 依赖）。
    void initAt(const Position& head, Direction dir, int len);

    void set_speed(int speed) { this->speed = speed; }            // 修改速度（内联）
    void set_stamina(int stamina) { this->stamina = stamina; }    // 修改体力（内联）
    // 加速键（R / Boost1 / Boost2）：在“起步速度”与“加速速度”两档间切换，
    // 与单人模式一致，不做多档循环。体力耗尽（canBoost == false）时忽略加速请求，
    // 直到恢复体力 > 10 点才重新允许（恢复逻辑在 move() 中维护）。内联。
    void toggleSpeed()
    {
        if (!canBoost) return;   // 体力耗尽后禁止加速，保持当前速度档位
        int start = GameSpec::getStartSpeed();
        int boost = GameSpec::getBoostSpeed();
        speed = (speed == start) ? boost : start;
    }

    inline bool canBoostSpeed() const   // 是否允许加速（体力耗尽禁止，恢复>10点重新允许）
    {
        return canBoost;
    }

    // 从存档恢复蛇的完整状态（引擎“继续游戏”调用）
    void restore(const std::deque<Position>& body,
                 Direction dir,
                 bool growing,
                 bool dead,
                 const Position& newHead,
                 const Position& oldTail,
                 int speed,
                 int stamina);


    void setgrowing()                       // 设置成长中
    {
        this->growing = true;
    }

    void setdead()                          // 设置死亡
    {
        this->dead = true;
    }

    inline const bool isGrowing() const     // 检查蛇是否在成长中
    {
        return growing;
    }

    inline bool is_dead() const             // 检查蛇是否死亡
    {
        return dead;
    }

    inline Position get_new_head() const    // 获取蛇的新头部位置
    {
        return new_head;
    }

    inline const Position& getOldTail() const           // 获取蛇的旧尾部位置
    {
        return old_tail;
    }

    inline int getLength() const            // 获取蛇的长度（= body 实际格子数）
    {
        return static_cast<int>(body.size());
    }

    inline int getSpeed() const             // 获取蛇的速度
    {
        return speed;
    }

    inline int getStamina() const           // 获取蛇的体力
    {
        return stamina;
    }

    inline bool isBoosting() const          // 是否处于加速状态（速度 == 当前配置的加速档）
    {
        return speed == GameSpec::getBoostSpeed();
    }

    inline bool isSnakeBody(const Position& pos)  const // 检查是否在蛇身体上
    {
        for (int i = 1; i < body.size(); i++)
        {
            if (pos == body[i])
                return true;
        }

        return false;
    }

    inline const std::deque<Position>& getBody() const  // 获取蛇的身体
    {
        return body;
    }

    inline const Direction& getDirection() const        // 获取蛇的当前方向
    {
        return currentDirection;
    }

    // 颜色设置/获取（双人区分两条蛇）
    inline void setColors(int head, int body) { headColor = head; bodyColor = body; }
    inline int getHeadColor() const { return headColor; }
    inline int getBodyColor() const { return bodyColor; }



};











#endif 