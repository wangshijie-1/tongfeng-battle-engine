#ifndef TWO_PLAYER_MODE_H
#define TWO_PLAYER_MODE_H

#include "Snake.h"
#include "board.h"
#include "food.h"
#include "render.h"
#include "Screen.h"
#include "World_Unit.h"
#include "KeyInput.h"
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>
#include <vector>      // 准备阶段提示文字的像素缓存

// 双人模式：双线程，两条蛇各占一个线程，按自己的速度（累计时间）独立移动；
// 主线程（引擎）按固定帧率(10ms)调用 render() 只“监视”两条蛇的变化并增量绘制。
//
// 复用成熟范式（直接照抄单人）：
//   - Snake::move() 自身逻辑不动；
//   - 撞墙 / 自撞判定照抄 SingleMode::checkCollision；
//   - 食物生成照抄 SingleMode::spawnFood；
// 双人特有、不能照抄的部分才自己写：
//   - 撞对方身体 / 两蛇头相撞 → 双死；
//   - 两条蛇各自独立加速（speed 不同 → interval 不同）；
//   - 死亡判定只在“某条蛇真正移动之后”进行（绝不对未移动的帧用预测头判死）。
class TwoPlayerMode
{
public:
    TwoPlayerMode(Screen* s = nullptr);
    ~TwoPlayerMode();

    // 接收引擎分发的按键，改变对应蛇方向/加速
    void handleInput(Function key);

    // 逻辑更新：移动由各蛇线程完成，这里为空（兼容引擎节拍）
    void update();

    // 渲染：首帧完整清屏一次；之后每帧只重绘“发生变化”的蛇头/尾与食物（监视变化）
    void render_full();
    void render();

    // 是否游戏结束（一方/双方死亡）
    bool isGameOver() const { return gameOver.load(); }

    // 获胜方：0=平局，1=P1，2=P2
    int getWinner() const { return winner.load(); }

    // 重置为新对局（先停旧线程，重建棋盘与蛇，再启动新线程）
    void resetGame();

    // 停止蛇线程（离开双人模式时由引擎调用）
    void stop();

    // 暂停/恢复蛇移动线程：暂停时线程不退出、只是冻结（不累计不移动），恢复时继续。
    // 用于双人模式“暂停不残留旧蛇”——单人模式移动由节拍驱动，无需此接口。
    void setPaused(bool p) { paused.store(p); }

private:
    void initGame();
    void spawnFood();                                       // 照抄单人食物生成
    bool checkCollision(Snake* self, Snake* other);         // 撞墙/自撞照抄 + 撞对方身
    void finalize();                                        // 结算 winner / gameOver
    void snakeThread(Snake* self, Snake* other,
                     std::atomic<bool>* selfDead,
                     std::atomic<bool>* otherDead);          // 单条蛇的移动线程
    void stopThreads();

    // 草地渲染辅助：照抄单人成熟增量渲染（render_snake_food 逻辑），
    // 每帧无条件擦旧尾+画头+画头后一节，各蛇用专属颜色。
    void drawSnakeMature(Snake* self, int headColor, int bodyColor);
    // 右侧栏：只更新两条蛇长度数字（背景/标签由 render_full 初始化）
    void drawSideNumbers();

    Screen* screen = nullptr;

    Board board;
    Snake snake1;
    Snake snake2;
    Food food;
    ScreenRender renderer;

    std::mutex mtx;                  // 保护两蛇 body / food / board 标记 / 结算状态
    std::thread thread1, thread2;
    std::atomic<bool> running{false};
    std::atomic<bool> paused{false};   // 暂停时冻结蛇移动线程（不退出、不累计不移动）
    std::atomic<bool> started{false};   // 准备阶段：进入后蛇不动，任一方按方向键才开始
    std::atomic<bool> dead1{false};
    std::atomic<bool> dead2{false};
    std::atomic<bool> gameOver{false};
    std::atomic<int>  winner{0};

    static const int FRAME_MS = 10;   // 草地监视/线程累计刷新间隔(ms)；类外有定义

    // 草地增量渲染照抄单人：记录“上一帧食物位置”，用于吃食物后擦除残留的黄色像素
    Position lastFoodPos{ -1, -1 };

    // 准备阶段提示文字：进入时记录覆盖区域的原始像素，开始后一次性还原（不每帧重绘）
    void showStartHint();
    void clearStartHint();
    bool hintActive{false};
    int hintX{0}, hintY{0}, hintW{0}, hintH{0};
    std::vector<int> hintBuf;
};

#endif
