#ifndef MENUMODE_H
#define MENUMODE_H

#include "World_Unit.h"
#include "Screen.h"
#include "KeyInput.h"
#include "Font.h"   // 复用极简位图字体，绘制中文菜单文字
#include <vector>

class SingleMode;   // 前置声明：MenuMode 仅在确认清档时通过指针调用

// 菜单项枚举
enum class MenuItem
{
    SINGLE_PLAYER,
    TWO_PLAYER,
    CONTINUE_GAME,
    CONFIG,
    COUNT
};

// 菜单模式：渲染菜单、处理上下确认按键
class MenuMode
{
public:
    MenuMode(Screen* screen, bool hasSaveData);
    ~MenuMode();

    // 处理按键：上/下切换选项，确认执行
    // 返回：选中的菜单动作，MenuItem::COUNT 表示无动作
    MenuItem handleInput(Function key);

    // 渲染菜单
    void render();

    // 更新存档状态（外部调用）
    void setSaveDataExists(bool exists);

    // 关联 SingleMode（GameEngine 构造完成后注入，用于确认清档）
    void setSingleMode(SingleMode* s);

private:
    // 绘制单个菜单项文字（含选中高亮 / 无存档置灰）
    void drawMenuItem(int index, bool selected, bool disabled, int y);
    void drawTitle();
    void clearScreen();
    void renderConfigPanel();     // 配置面板（右侧 124px 区域）
    void renderMenuItems(bool highlightSelected = true);  // 正常菜单选项列表
    void clearScreenArea(int x, int y, int w, int h);  // 清屏某矩形区域
    MenuItem handleMenuInput(Function key);     // 普通菜单输入处理
    MenuItem handleConfigInput(Function key);   // 配置面板输入处理
    MenuItem handleConfirmInput(Function key);  // 修改配置确认弹窗输入处理

    // 进入配置面板：将草稿同步为当前生效配置
    void enterConfig();
    // 弹出“修改将失去存档”确认框（先缓存弹窗区域像素）
    void showConfirmDialog();
    // 关闭弹窗；apply=true 时把草稿写回 GameSpec 并清档，并关闭右侧面板
    void closeConfirmDialog(bool apply);
    void cacheDialogArea();
    void restoreDialogArea();
    void renderConfirmDialog();

    Screen* scr;
    SingleMode* singleModePtr = nullptr;  // 由引擎注入，确认清档时调用
    MenuItem selectedItem;
    bool hasSaveData;  // 是否有存档，决定"继续游戏"是否可选
    bool configOpen;   // 是否打开配置面板

    // 配置草稿：编辑期间只改草稿，确认后才写回 GameSpec
    int draftCellSizeIndex = 0;
    int draftStartSpeedIndex = 0;
    int draftBoostSpeedIndex = 0;

    // 修改确认弹窗状态
    bool confirmDialog = false;
    std::vector<int> dlgCache;          // 弹窗覆盖区域的像素缓存（用于关闭时还原）
    int dlgX = 0, dlgY = 0, dlgW = 0, dlgH = 0;

    // 屏幕尺寸缓存
    int screenW;
    int screenH;
};

#endif