#ifndef SINGLE_MODE_H
#define SINGLE_MODE_H

#include "Snake.h"
#include "board.h"
#include "food.h"
#include "render.h"
#include "Screen.h"
#include "rand_helper.h"
#include "World_Unit.h"
#include "KeyInput.h"   // 复用按键输入模块，Function 枚举来源
#include <chrono>

// 单人模式：纯逻辑，支持存档/恢复
class SingleMode
{
public:
    SingleMode();
    ~SingleMode();

    // 接收引擎分发的按键，改变蛇方向
    void handleInput(Function key);

    // 逻辑更新（移动、碰撞、吃食物等）
    void update();

    // 渲染：首帧用完整渲染（清屏+画整块棋盘），之后每帧用增量渲染（仅蛇与食物）
    void render_full();   // 完整渲染，清屏并重绘整个棋盘
    void render();        // 增量渲染，只更新蛇与食物（首帧后每帧调用）

    // 按下 R 键时切换蛇速度（10↔20），并通知引擎刷新 tickInterval
    void boostSpeed();

    // 获取蛇速度（引擎据此计算 tick 间隔）
    int getSnakeSpeed() const { return snake.getSpeed(); }

    // 获取当前得分（蛇身长度），用于暂停/结算画面显示
    int getScore() const { return snake.getLength(); }

    // 获取历史最高分
    static int getHighScore();
    // 更新历史最高分（在得分超过时调用）
    static void updateHighScore(int score);

    // 是否游戏结束
    bool isGameOver() const { return gameOver; }

    // 是否获胜（单人模式：蛇长 == 有效草地格数）
    bool isWin() const { return won; }

    // 有效草地格子数（建局时纯计算一次，存档恢复后重算）
    int getFreeCells() const { return freeCells; }

    // 存档：保存当前完整游戏状态
    void saveState();

    // 恢复：加载存档状态
    void loadState();

    // 是否有有效存档
    bool hasValidSave() const { return hasSave; }

    // 清除存档（不重置对局逻辑）：用于“修改配置将失去存档”
    void clearSave();

    // 重置为新游戏
    void resetGame();

    // 食物存活时长（毫秒）：超过该时长后自动消失并重新随机生成
    static const int FOOD_LIFETIME_MS = 20000;

    // 根据当前 GameSpec 配置重新创建 Board（格子大小变更时调用）
    void recalcBoard();

private:
    void initGame();
    void spawnFood();
    bool checkCollision();

    Screen* scr;

    Board board;
    Snake snake;
    Food food;
    ScreenRender renderer;

    Position lastFoodPos{ 0, 0 };   // 上一帧食物位置，用于增量渲染时擦除旧食物像素

    std::chrono::steady_clock::time_point foodSpawnTime;   // 当前食物的生成时间，用于 20 秒自动刷新

    bool gameOver = false;
    bool hasSave = false;

    int freeCells = 0;   // 有效草地格子数（非墙格），蛇填满即获胜
    bool won = false;    // 获胜标志

    // 存档数据结构
    struct SaveData
    {
        std::deque<Position> snakeBody;
        Direction snakeDirection;
        bool snakeGrowing;
        bool snakeDead;
        Position snakeNewHead;
        Position snakeOldTail;
        int snakeSpeed;
        int snakeStamina;

        Position foodPos;
        bool foodExist;

        // Board 网格状态（仅非空单元格）
        struct CellRecord { Position pos; CellType type; };
        std::vector<CellRecord> boardCells;

        bool win = false;       // 是否已获胜（单人填满棋盘）
        bool valid = false;
    };
    SaveData saveData;
};

#endif