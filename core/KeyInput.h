#ifndef KEYINPUT_H
#define KEYINPUT_H

#include <array>
#include <vector>
#include <termios.h>

enum class Function
{
    MENU,               // 菜单
    SINGLE_PLAYER,      // 单人游戏
    TWO_PLAYER,         // 双人游戏
    GAME_OVER,          // 游戏结束
    Pause,              // 暂停游戏
    Resume,             // 继续游戏
    Restart,            // 重新开始游戏
    Quit,               // 退出游戏
    Up,                 // P1上 W
    Down,               // P1下 S
    Left,               // P1左 A
    Right,              // P1右 D
    UpTwo,              // P2上 ↑
    DownTwo,            // P2下 ↓
    LeftTwo,            // P2左 ←
    RightTwo,           // P2右 →
    Confirm,            // 确认键
    Back,               // 返回/取消键（ESC；在确认弹窗中表示"取消"）
    Boost1,             // 加速键1（P1，R 键触发）
    Boost2,             // 加速键2（P2，/ 键触发）
    None                // ✅ 无效占位
};

class KeyInput
{
public:
    KeyInput();
    ~KeyInput();

    // 原有：终端ANSI转义读取（控制台调试）
    const std::array<Function, 2>& pollKeys();

    // 新增：自动检索键盘evdev，支持热插拔、event编号自动适配
    const std::array<Function, 2>& pollKeysEvdevAuto();

    void clearQueue();

private:
    struct termios old_termios;
    std::array<Function, 2> key_queue;

    // evdev 自动检索相关成员
    int ev_fd;
    bool hasKey(Function f) const;
    Function evdevKeyToFunc(int code) const;

    // 内部工具：扫描寻找可用键盘event设备，成功返回fd，失败返回-1
    int findKeyboardDevice();
    // 关闭旧fd，重置
    void closeEvFd();
};

#endif