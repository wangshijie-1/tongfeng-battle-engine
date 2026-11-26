#include <cstdio>
#include <unistd.h>
#include <array>
#include <fcntl.h>
#include <errno.h>
#include <cstring>
#include <dirent.h>
#include <linux/input.h>
#include "KeyInput.h"

KeyInput::KeyInput()
{
    tcgetattr(STDIN_FILENO, &old_termios);
    struct termios raw = old_termios;
    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);

    key_queue.fill(Function::None);
    ev_fd = -1;
}

KeyInput::~KeyInput()
{
    tcsetattr(STDIN_FILENO, TCSANOW, &old_termios);
    closeEvFd();
}

void KeyInput::closeEvFd()
{
    if (ev_fd >= 0)
    {
        close(ev_fd);
        ev_fd = -1;
    }
}

bool KeyInput::hasKey(Function f) const
{
    return key_queue[0] == f || key_queue[1] == f;
}

void KeyInput::clearQueue()
{
    key_queue.fill(Function::None);
}

// ====================== 原有终端读取函数 【无修改】 ======================
const std::array<Function, 2>& KeyInput::pollKeys()
{
    char c;
    if (read(STDIN_FILENO, &c, 1) != 1)
    {
        return key_queue;
    }

    Function new_key = Function::None;
    switch (c)
    {
    case 'w': case 'W': new_key = Function::Up; break;
    case 's': case 'S': new_key = Function::Down; break;
    case 'a': case 'A': new_key = Function::Left; break;
    case 'd': case 'D': new_key = Function::Right; break;
    case 'm': new_key = Function::MENU; break;
    case '1': new_key = Function::SINGLE_PLAYER; break;
    case '2': new_key = Function::TWO_PLAYER; break;
    case 'q': case 'Q': new_key = Function::Quit; break;
    case 'p': case 'P': new_key = Function::Pause; break;
    case 'r': case 'R': new_key = Function::Boost1; break;     // R = P1 加速
    case 'o': case 'O': new_key = Function::Resume; break;
    case '\r': case '\n': new_key = Function::Confirm; break;  // 回车键 = 确认
    case '/': new_key = Function::Boost2; break;               // / = P2 加速
    case 0x1B:
    {
        // 裸 ESC（后面不是 '['）→ 返回/取消键（Back）
        if (read(STDIN_FILENO, &c, 1) != 1 || c != '[')
        {
            new_key = Function::Back;
            break;
        }
        if (read(STDIN_FILENO, &c, 1) != 1)
            return key_queue;

        switch (c)
        {
        case 'A': new_key = Function::UpTwo; break;
        case 'B': new_key = Function::DownTwo; break;
        case 'C': new_key = Function::RightTwo; break;
        case 'D': new_key = Function::LeftTwo; break;
        default: new_key = Function::None; break;
        }
        break;
    }
    default:
        new_key = Function::None;
        break;
    }

    if (new_key == Function::None || hasKey(new_key))
    {
        return key_queue;
    }

    key_queue[0] = key_queue[1];
    key_queue[1] = new_key;
    return key_queue;
}

// ====================== evdev 辅助：内核键码转Function ======================
Function KeyInput::evdevKeyToFunc(int code) const
{
    switch (code)
    {
    case KEY_W: return Function::Up;
    case KEY_S: return Function::Down;
    case KEY_A: return Function::Left;
    case KEY_D: return Function::Right;

    case KEY_M: return Function::MENU;
    case KEY_1: return Function::SINGLE_PLAYER;
    case KEY_2: return Function::TWO_PLAYER;
    case KEY_Q: return Function::Quit;
    case KEY_P: return Function::Pause;
    case KEY_R: return Function::Boost1;        // R = P1 加速
    case KEY_O: return Function::Resume;
    case KEY_ENTER: return Function::Confirm;  // 回车键 = 确认

    case KEY_UP:    return Function::UpTwo;
    case KEY_DOWN:  return Function::DownTwo;
    case KEY_LEFT:  return Function::LeftTwo;
    case KEY_RIGHT: return Function::RightTwo;

    case KEY_ESC:   return Function::Back;  // ESC = 返回/取消键

    case KEY_SLASH: return Function::Boost2;  // / = P2 加速

    default: return Function::None;
    }
}

// ====================== 自动扫描：寻找可用键盘event设备 ======================
int KeyInput::findKeyboardDevice()
{
    DIR* dir = opendir("/dev/input");
    if (!dir)
    {
        perror("opendir /dev/input failed");
        return -1;
    }
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr)
    {
        if (strncmp(entry->d_name, "event", 5) != 0)
            continue;

        char path[256];
        snprintf(path, sizeof(path), "/dev/input/%s", entry->d_name);
        int fd = open(path, O_RDONLY | O_NONBLOCK);
        if (fd < 0)
            continue;

        // 判断是否具备键盘按键能力 EV_KEY
        unsigned long key_bit[KEY_MAX / 8 + 1] = { 0 };
        if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(key_bit)), key_bit) >= 0)
        {
            // 包含字母按键，判定为键盘
            if (key_bit[KEY_A / 8] & (1UL << (KEY_A % 8)))
            {
                closedir(dir);
                return fd;
            }
        }
        close(fd);
    }
    closedir(dir);
    return -1;
}

// ====================== 自动evdev读取主接口（热插拔支持） ======================
const std::array<Function, 2>& KeyInput::pollKeysEvdevAuto()
{
    // fd失效：设备拔掉、出错，先关闭旧句柄，重新检索
    if (ev_fd < 0)
    {
        ev_fd = findKeyboardDevice();
        if (ev_fd < 0)
        {
            return key_queue;
        }
    }

    struct input_event ev;
    ssize_t ret;
    while ((ret = read(ev_fd, &ev, sizeof(struct input_event))) == sizeof(struct input_event))
    {
        if (ev.type == EV_KEY && ev.value == 1)
        {
            Function new_key = evdevKeyToFunc(ev.code);
            if (new_key == Function::None || hasKey(new_key))
                continue;

            key_queue[0] = key_queue[1];
            key_queue[1] = new_key;
        }
    }

    // read 返回 <0，区分是不是正常无数据
    if (ret < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
    {
        // 设备断开，关闭fd，下一帧自动重扫
        closeEvFd();
    }

    return key_queue;
}