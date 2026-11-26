#ifndef __GAMEENGINE_H__
#define __GAMEENGINE_H__

#include <chrono>
#include <vector>
#include "modes/SingleMode.h"
#include "modes/MenuMode.h"
#include "modes/TwoPlayerMode.h"
#include "KeyInput.h"
#include "World_Unit.h"
#include "Screen.h"

// ============================================================================
// 按键使用说明
// ----------------------------------------------------------------------------
// 键盘输入统一由 KeyInput 采集（2 元素队列、新键覆盖旧键），
// 引擎按当前 GameState 选择对应 handleInput_* 处理器执行。
// 下列表格按【你实际要按的物理按键】列出每个键在各个游戏状态下的功能。
//
// +----------+------------------+--------------------+-------------------+-----------------+---------------+------------------+------------------+
// | 物理按键 | 主菜单 (MENU)     | 单人游戏 (SINGLE)   | 配置面板 (CONFIG)  | 双人游戏 (TWO) | 暂停 (PAUSED) | 游戏结束 (OVER) | 确认弹窗 (DLG)    |
// +----------+------------------+--------------------+-------------------+-----------------+---------------+------------------+------------------+
// | W        | 上移选中菜单项    | 蛇向上转向          | 上移选配置项       | P1 向上转向     | —             | —                | —                |
// | S        | 下移选中菜单项    | 蛇向下转向          | 下移选配置项       | P1 向下转向     | —             | —                | —                |
// | A        | 无                | 蛇向左转向          | 无                | P1 向左转向     | —             | —                | —                |
// | D        | 无                | 蛇向右转向          | 无                | P1 向右转向     | —             | —                | —                |
// | ↑(方向)  | 上移选中菜单项    | 蛇向上转向          | 上移选配置项       | P2 向上转向     | —             | —                | —                |
// | ↓(方向)  | 下移选中菜单项    | 蛇向下转向          | 下移选配置项       | P2 向下转向     | —             | —                | —                |
// | ←(方向)  | 无                | 蛇向左转向          | 减小当前配置值     | P2 向左转向     | —             | —                | —                |
// | →(方向)  | 无                | 蛇向右转向          | 增大当前配置值     | P2 向右转向     | —             | —                | —                |
// | 空格     | 无                | 无                  | 无                | （无）          | —             | —                | —                |
// | R        | 无                | R 键加速（档位循环） | 无                | P1 加速         | —             | —                | —                |
// | /        | 无                | 无                  | 无                | P2 加速         | —             | —                | —                |
// | 1        | 开始单人对局      | —                   | —                 | —               | —             | —                | —                |
// | 2        | 开始双人模式      | —                   | —                 | —               | —             | —                | —                |
// | 回车     | 确认选中菜单项    | —                   | 确认修改/关面板    | —               | 回主菜单       | 回主菜单         | 确认应用(清存档)  |
// | P        | 无                | 暂停对局            | 退出配置面板(取消) | 暂停对局         | 恢复对局       | —                | 取消(仅关弹窗)    |
// | O        | 无                | 暂停中恢复对局      | 无                | 恢复对局         | 恢复对局       | 回主菜单         | 无                |
// | M        | 无                | 保存并回主菜单      | 退出配置面板(取消) | 保存并回主菜单   | 保存并回主菜单  | 回主菜单         | 取消(仅关弹窗)    |
// | Q        | 无                | 直接退出游戏        | 无                | 直接退出游戏     | 直接退出游戏    | 直接退出游戏     | 无                |
// | R        | 无                | 无（预留）          | 无                | 无（预留）       | —             | —                | —                |
// | ESC      | 无                | 无                  | 退出配置面板(取消) | 无              | 无             | 无               | 取消键(仅关弹窗)  |
// +----------+------------------+--------------------+-------------------+-----------------+---------------+------------------+------------------+
//
// 说明：
//  - "双人游戏 (TWO)"：P1 用 WASD 控制（火山红蛇），P2 用方向键控制（阳光黄蛇）；
//    R=P1 加速（与单人一致），/=P2 加速（两蛇独立加速、互不影响，均遵循配置起步/加速速度）；
//    P=暂停，M=保存并回主菜单，Q=退出。两蛇对角出生，撞墙/撞任意蛇身/头碰头均判死，
//    一方死亡对方即胜，同时死亡比长度定胜负（等长则平局）。
//  - "暂停 (PAUSED)"列的"—"表示该状态下对应按键无效。
//  - 无存档时"继续游戏"选项在菜单中置灰不可选（由 MenuMode 内部处理）。
//  - "配置面板 (CONFIG)"：进入后 ←/→ 调整当前配置项；回车=确认并关闭面板（改动后先弹确认框）；
//    ESC / M / P = 取消并直接退出面板，未确认修改不生效。
//  - "确认弹窗 (DLG)"：仅在配置改动后弹出。回车=确认应用（写回配置并清除存档，右侧面板消失）；
//    取消键(ESC / M / P)=仅关闭弹窗，保留草稿与配置面板，可继续调整。
// ============================================================================

// 游戏状态
enum class GameState
{
    MENU,
    SINGLE_PLAYER,
    TWO_PLAYER,
    PAUSED,
    GAME_OVER
};

// 暂停遮罩像素缓存
struct PausePixelCache
{
    std::vector<int> pixels;   // 缓存的原始像素
    int x, y, w, h;            // 缓存区域
    bool valid = false;        // 缓存是否有效

    void clear() { pixels.clear(); valid = false; }
};

// 游戏引擎：统一输入轮询、状态机分发、节拍驱动、存档管理、暂停缓存
class GameEngine
{
public:
    GameEngine();
    ~GameEngine();

    void run();                 // 主循环

private:
    // === 输入处理器（按状态分发） ===
    void processInput();                    // 统一入口，分发给下面的处理器
    void handleInput_Menu(Function key);    // 菜单状态按键处理
    void handleInput_SinglePlayer(Function key);  // 单人游戏按键处理
    void handleInput_TwoPlayer(Function key);     // 双人游戏按键处理（预留）
    void handleInput_Paused(Function key);    // 暂停状态按键处理
    void handleInput_GameOver(Function key);  // 游戏结束按键处理

    // === 状态更新 ===
    void updateCurrentMode();               // 按节拍驱动当前模式 update/render

    // 根据速度刷新节拍间隔（见实现）：useStartSpeed=true 用配置起步速度
    // （进入对局/配置后重置），false 用蛇当前速度（加速/读档/每帧）。集中此处避免多处重复。
    void refreshTick(bool useStartSpeed);

    // === 菜单动作 ===
    void onMenuSelect_SinglePlayer();       // 开启新单人对局
    void onMenuSelect_TwoPlayer();          // 双人模式（预留）
    void onMenuSelect_ContinueGame();       // 继续游戏（恢复存档）
    void onMenuSelect_Config();             // 应用配置并重绘菜单

    // 进入主菜单：设置状态、刷新存档可用标志、绘制一次菜单。
    // 菜单只在"进入"和"按键"时重绘，避免逐帧屏闪（见 handleInput_Menu）。
    void enterMenu();

    // 绘制游戏结束画面（文字提示 + 得分）
    void drawGameOver();

    // === 存档管理 ===
    void saveCurrentGame();                 // 保存当前对局（切菜单/暂停退出时自动调用）
    bool loadSavedGame();                   // 加载存档恢复对局

    // === 暂停机制（像素缓存还原） ===
    void enterPause();                      // 进入暂停：缓存像素、绘制遮罩
    void exitPause();                       // 退出暂停：还原像素
    void cachePauseArea();                  // 缓存暂停遮罩覆盖区域原始像素
    void restorePauseArea();                // 还原缓存像素
    void drawPauseOverlay();                // 绘制半透明暂停提示矩形

    // === 核心成员 ===
    KeyInput keyInput;              // 唯一输入源
    Screen* scr;                    // 屏幕单例

    SingleMode singleMode;          // 单人模式实例
    MenuMode menuMode;              // 菜单模式实例
    TwoPlayerMode twoPlayerMode;    // 双人模式实例（空壳）

    GameState currentState = GameState::MENU;  // 默认进入主菜单
    GameState prevGameState = GameState::MENU; // 暂停前的游戏状态
    bool gameOver = false;

    // 节拍控制：tickInterval = GAME_SPEED(7000) / snakeSpeed；由构造函数按配置初始化
    std::chrono::steady_clock::time_point lastTick;
    int tickInterval;

    // 暂停像素缓存
    PausePixelCache pauseCache;

    // 屏幕尺寸缓存
    int screenW = 0;
    int screenH = 0;
};

#endif
