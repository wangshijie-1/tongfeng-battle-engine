#ifndef WORLD_UNIT_H
#define WORLD_UNIT_H

//坐标
struct Position
{
    int x;
    int y;

    bool operator==(const Position& other) const
    {
        return x == other.x && y == other.y;
    }

    Position operator+(const Position& other) const
    {
        return { x + other.x, y + other.y };
    }
};

//方向
enum class Direction
{
    UP,
    DOWN,
    LEFT,
    RIGHT,
    NONE
};

//细胞类型
enum CellType
{
    EMPTY,
    SNAKE_BODY,
    SNAKE_HEAD,
    FOOD,
    WALL,
    WU
};


static int Color[CellType::WU] =
{
    0xFF00,
    0x00D4AA,       //全息青
    0xFF0000,       //火山红
    0xFFFF00,       //脉冲黄
    0XFF,           //纯蓝
};


// 游戏规格：布局常量、格子配置、速度配置集中管理
struct GameSpec
{
    // 布局常量
    static constexpr int BOARD_RENDER_W = 900;  // 活动棋盘渲染宽度（仅作兜底参考，实际以真实屏幕为准）
    static constexpr int PANEL_W = 124;         // 右侧分数侧栏宽度，需从棋盘可用宽度中扣除
    static constexpr int SCREEN_H_DEFAULT = 600; // 默认屏幕高度，实际以 Screen 为准
    static constexpr int GAME_SPEED = 7000;       // tickInterval = GAME_SPEED / snakeSpeed

    // 格子尺寸配置（三个档位，单位 px；gap 固定为 1）
    static const int cellSizes[3];       // 格子像素大小：19, 29, 49
    static int cellOptionsCount() { return 3; }

    // 速度配置（档位列表）
    static const int startSpeedOptions[3];
    static const int boostSpeedOptions[3];
    static int startSpeedOptionsCount() { return 3; }
    static int boostSpeedOptionsCount() { return 3; }

    // 运行时当前配置索引
    static int cellSizeIndex;
    static int startSpeedIndex;
    static int boostSpeedIndex;

    // 派生值（纯计算，无状态依赖）
    static inline int getCellSize()  { return cellSizes[cellSizeIndex]; }
    static inline int getGap()       { return 1; }
    static inline int getCellStep()  { return getCellSize() + getGap(); }
    // 棋盘列数必须依据【真实屏幕宽度】计算，而非固定 900。
    // 若写成 BOARD_RENDER_W / getCellStep()，在窄于 900px 的屏幕上，右侧格子会超出
    // 帧缓冲区宽度，draw_retangle 的像素索引发生换行，食物因此“错位/出现在两个格子
    // 中间”，且蛇走到逻辑格也吃不到（渲染位置已换行）。这里扣除右侧 124px 分数侧栏。
    static inline int getGridW(int screenW)
    {
        int avail = (screenW > PANEL_W) ? (screenW - PANEL_W) : (BOARD_RENDER_W - PANEL_W);
        return avail / getCellStep();
    }
    static inline int getGridH(int screenH = SCREEN_H_DEFAULT) { return screenH / getCellStep(); }
    static inline int getStartSpeed(){ return startSpeedOptions[startSpeedIndex]; }
    static inline int getBoostSpeed(){ return boostSpeedOptions[boostSpeedIndex]; }
};

#endif