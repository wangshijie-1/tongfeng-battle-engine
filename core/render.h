#ifndef RENDER_H
#define RENDER_H

#include "board.h"
#include "Snake.h"
#include "food.h"
#include "Screen.h"
#include "World_Unit.h"

// 配置信息
constexpr int BOARD_X0 = 0;
constexpr int BOARD_Y0 = 0;

class ScreenRender
{
public:
    // 全屏渲染
    void render(const Board& board, const Snake& Snake,
        const Position& food, Screen* scr);

    // 增量渲染蛇与食物
    void render_snake_food(const Board& board, const Snake& snake,
        const Position& food, Screen* scr);

    // 按蛇自身颜色，整条绘制一条蛇（双人模式首帧区分两蛇）
    void renderSnake(const Board& board, const Snake& snake, Screen* scr);

    // 绘制游戏右侧分数侧栏（124px 宽）：首次初始化（背景+边框+标签+数字）
    void renderScorePanel(Screen* scr, int currentScore, int highScore);
    // 仅更新数字区域（背景/边框/标签已存在时调用，避免整块重绘覆盖游戏画面）
    void renderScoreOnly(Screen* scr, int currentScore, int highScore);

    // 绘制侧栏体力区：标签 STA（青色）+ 数值 "当前/上限"（按剩余比例着色），居中。
    // y 为标签行坐标，数值画在其下方 12px。maxStamina/currentStamina 由调用方传入。
    void renderStaminaPanel(Screen* scr, int y, int maxStamina, int currentStamina);
    // 仅更新体力数值区域（标签已存在时每帧调用，避免整块重绘覆盖游戏画面）
    void renderStaminaOnly(Screen* scr, int y, int maxStamina, int currentStamina);

private:
    void clearScreen(void) {}
};

#endif