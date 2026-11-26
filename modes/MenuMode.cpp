#include "MenuMode.h"
#include "SingleMode.h"   // 完整类型：确认清档时调用 SingleMode::clearSave
#include <algorithm>
#include <cstdio>

MenuMode::MenuMode(Screen* screen, bool hasSaveData) :
    scr(screen),
    selectedItem(MenuItem::SINGLE_PLAYER),
    hasSaveData(hasSaveData),
    configOpen(false),
    screenW(0),
    screenH(0)
{
    if (scr)
    {
        screenW = scr->getWidth();
        screenH = scr->getHeight();
    }
}

MenuMode::~MenuMode()
{
}

// ─────────────────────────────────────────────
// 输入处理
// ─────────────────────────────────────────────
MenuItem MenuMode::handleInput(Function key)
{
    if (confirmDialog)
        return handleConfirmInput(key);
    if (configOpen)
        return handleConfigInput(key);
    return handleMenuInput(key);
}

MenuItem MenuMode::handleMenuInput(Function key)
{
    switch (key)
    {
    case Function::Up:
    case Function::UpTwo:
        selectedItem = static_cast<MenuItem>(
            (static_cast<int>(selectedItem) - 1 + static_cast<int>(MenuItem::COUNT)) % static_cast<int>(MenuItem::COUNT));
        // 跳过无存档时的"继续游戏"
        if (selectedItem == MenuItem::CONTINUE_GAME && !hasSaveData)
            selectedItem = static_cast<MenuItem>(
                (static_cast<int>(selectedItem) - 1 + static_cast<int>(MenuItem::COUNT)) % static_cast<int>(MenuItem::COUNT));
        break;

    case Function::Down:
    case Function::DownTwo:
        selectedItem = static_cast<MenuItem>(
            (static_cast<int>(selectedItem) + 1) % static_cast<int>(MenuItem::COUNT));
        if (selectedItem == MenuItem::CONTINUE_GAME && !hasSaveData)
            selectedItem = static_cast<MenuItem>(
                (static_cast<int>(selectedItem) + 1) % static_cast<int>(MenuItem::COUNT));
        break;

    case Function::Confirm:
        if (selectedItem == MenuItem::CONTINUE_GAME && !hasSaveData)
            return MenuItem::COUNT;
        // 选中配置 → 打开配置面板（不返回动作，由 render 切换到配置模式）
        if (selectedItem == MenuItem::CONFIG)
        {
            enterConfig();
            return MenuItem::COUNT;
        }
        return selectedItem;

    default:
        break;
    }
    return MenuItem::COUNT;
}

MenuItem MenuMode::handleConfigInput(Function key)
{
    // 0=格子大小  1=起步速度  2=加速速度
    static const int configCount = 3;
    static const int optCount[3] = { 3, 3, 3 };
    static int* optDraftPtr[3] = { &draftCellSizeIndex,
                                   &draftStartSpeedIndex,
                                   &draftBoostSpeedIndex };

    switch (key)
    {
    case Function::Up:
    case Function::UpTwo:
        // 切换到上一行配置
        selectedItem = static_cast<MenuItem>(
            (static_cast<int>(selectedItem) - 1 + configCount) % configCount);
        break;

    case Function::Down:
    case Function::DownTwo:
        // 切换到下一行配置
        selectedItem = static_cast<MenuItem>(
            (static_cast<int>(selectedItem) + 1) % configCount);
        break;

    case Function::Left:
    case Function::LeftTwo:
        // 减小当前选项的草稿值（不直接写回 GameSpec）
        if (selectedItem >= MenuItem::SINGLE_PLAYER && selectedItem < MenuItem::COUNT)
        {
            int idx = static_cast<int>(selectedItem) - static_cast<int>(MenuItem::SINGLE_PLAYER);
            int& val = *optDraftPtr[idx];
            val = (val - 1 + optCount[idx]) % optCount[idx];
        }
        break;

    case Function::Right:
    case Function::RightTwo:
        // 增大当前选项的草稿值（不直接写回 GameSpec）
        if (selectedItem >= MenuItem::SINGLE_PLAYER && selectedItem < MenuItem::COUNT)
        {
            int idx = static_cast<int>(selectedItem) - static_cast<int>(MenuItem::SINGLE_PLAYER);
            int& val = *optDraftPtr[idx];
            val = (val + 1) % optCount[idx];
        }
        break;

    case Function::Confirm:
    {
        // 对比修改前后的配置：是否真的改了某项
        bool changed = (draftCellSizeIndex  != GameSpec::cellSizeIndex)  ||
                       (draftStartSpeedIndex != GameSpec::startSpeedIndex) ||
                       (draftBoostSpeedIndex != GameSpec::boostSpeedIndex);
        if (changed)
        {
            // 不一致 → 弹出确认框（先记录该处像素），不退出面板
            showConfirmDialog();
        }
        else
        {
            // 一致 → 直接退出配置面板，无任何弹窗
            configOpen = false;
            selectedItem = MenuItem::CONFIG;  // 退出后菜单光标停留在"配置"
        }
        return MenuItem::COUNT;
    }

    case Function::Pause:
    case Function::MENU:
    case Function::Back:
        // ESC / M / 返回键：放弃未确认修改，直接退出配置面板
        configOpen = false;
        selectedItem = MenuItem::CONFIG;  // 退出后菜单光标停留在"配置"
        return MenuItem::COUNT;

    default:
        break;
    }
    return MenuItem::COUNT;
}

// 进入配置面板：将草稿同步为当前生效配置（规避直接改写 GameSpec）
void MenuMode::enterConfig()
{
    configOpen = true;
    draftCellSizeIndex  = GameSpec::cellSizeIndex;
    draftStartSpeedIndex = GameSpec::startSpeedIndex;
    draftBoostSpeedIndex = GameSpec::boostSpeedIndex;
    // 默认选中第一个栏目（格子大小 = SINGLE_PLAYER）。
    selectedItem = MenuItem::SINGLE_PLAYER;
}

// 弹出“修改将失去存档”确认框：先记录弹窗覆盖区域的像素，再绘制弹窗
void MenuMode::showConfirmDialog()
{
    confirmDialog = true;
    // 弹窗尺寸贴合文案，居中显示
    dlgW = 320;
    dlgH = 96;
    dlgX = (screenW - dlgW) / 2;
    dlgY = (screenH - dlgH) / 2;
    cacheDialogArea();
}

// 关闭弹窗。apply=true：把草稿写回 GameSpec、清档、右侧面板消失；
//           apply=false：仅关弹窗，草稿丢弃，仍停留在配置面板（回到选择时的样子）
void MenuMode::closeConfirmDialog(bool apply)
{
    confirmDialog = false;
    restoreDialogArea();   // 先还原像素（弹窗消失）
    if (apply)
    {
        GameSpec::cellSizeIndex  = draftCellSizeIndex;
        GameSpec::startSpeedIndex = draftStartSpeedIndex;
        GameSpec::boostSpeedIndex = draftBoostSpeedIndex;
        if (singleModePtr) singleModePtr->clearSave();  // 失去存档 → “继续游戏”自然变灰不可继续
        hasSaveData = false;           // 同步菜单态，立即置灰“继续游戏”
        configOpen = false;            // 右侧配置面板随之消失
        selectedItem = MenuItem::CONFIG;
    }
    // apply=false 时保持 configOpen，草稿仍在，用户可继续调整或退出
}

// 记录弹窗覆盖区域的像素（按 int* 直接复制帧缓冲）
void MenuMode::cacheDialogArea()
{
    dlgCache.clear();
    if (!scr) return;
    int* fb = scr->getFrameBuffer();
    if (!fb) return;
    int w = scr->getWidth();
    dlgCache.reserve(dlgW * dlgH);
    for (int y = 0; y < dlgH; ++y)
    {
        int fy = dlgY + y;
        if (fy < 0 || fy >= scr->getHeight()) { for (int x = 0; x < dlgW; ++x) dlgCache.push_back(0); continue; }
        for (int x = 0; x < dlgW; ++x)
        {
            int fx = dlgX + x;
            if (fx < 0 || fx >= w) dlgCache.push_back(0);
            else dlgCache.push_back(fb[fy * w + fx]);
        }
    }
}

// 还原弹窗覆盖区域的像素
void MenuMode::restoreDialogArea()
{
    if (!scr) { dlgCache.clear(); return; }
    int* fb = scr->getFrameBuffer();
    if (!fb || dlgCache.empty()) { dlgCache.clear(); return; }
    int w = scr->getWidth();
    size_t i = 0;
    for (int y = 0; y < dlgH; ++y)
    {
        int fy = dlgY + y;
        if (fy < 0 || fy >= scr->getHeight()) { i += dlgW; continue; }
        for (int x = 0; x < dlgW; ++x)
        {
            int fx = dlgX + x;
            if (fx >= 0 && fx < w && i < dlgCache.size())
                fb[fy * w + fx] = dlgCache[i];
            ++i;
        }
    }
    dlgCache.clear();
}

// 绘制确认弹窗（提示修改配置将失去存档）
void MenuMode::renderConfirmDialog()
{
    if (!scr) return;
    // 半透明遮罩（简单压暗）：只在弹窗区域内轻微叠暗，保持轻量
    // 弹窗底
    scr->draw_retangle(dlgX, dlgY, dlgW, dlgH, 0x101820);
    // 边框（亮红，警示）
    scr->draw_retangle(dlgX, dlgY,                dlgW, 3,   0xFF4444);
    scr->draw_retangle(dlgX, dlgY + dlgH - 3,    dlgW, 3,   0xFF4444);
    scr->draw_retangle(dlgX, dlgY,                3,   dlgH, 0xFF4444);
    scr->draw_retangle(dlgX + dlgW - 3, dlgY,    3,   dlgH, 0xFF4444);

    const int scale = 1;   // 中文 16px，统一字号，确保两行均不超宽且清晰
    // 第一行（警示红）：修改配置将失去存档
    const char* line1 = "修改配置将失去存档";
    int w1 = Font::textWidth(line1, scale);
    Font::drawText(scr, dlgX + (dlgW - w1) / 2, dlgY + 20, line1, 0xFF6666, scale);

    // 第二行（灰）：操作提示，居中（两组之间用两空格分隔，清晰不挤）
    const char* line2 = "回车确认  取消键返回";
    int w2 = Font::textWidth(line2, scale);
    Font::drawText(scr, dlgX + (dlgW - w2) / 2, dlgY + 56, line2, 0xAAAAAA, scale);
}

// 确认弹窗输入：回车 = 确认应用；取消键 = 仅关闭弹窗
MenuItem MenuMode::handleConfirmInput(Function key)
{
    switch (key)
    {
    case Function::Confirm:
        closeConfirmDialog(true);
        return MenuItem::COUNT;   // 面板已关，引擎无需特殊处理

    case Function::Pause:
    case Function::MENU:
    case Function::Back:
        closeConfirmDialog(false);
        return MenuItem::COUNT;   // 仍停留在配置面板

    default:
        break;
    }
    return MenuItem::COUNT;
}

// ─────────────────────────────────────────────
// 渲染
// ─────────────────────────────────────────────
void MenuMode::render()
{
    if (!scr) return;
    clearScreen();
    // 进入配置后：保留左侧原始菜单界面，仅在右侧额外绘制配置选取面板（不再清屏只画右侧）
    renderMenuItems(!configOpen);
    if (configOpen)
        renderConfigPanel();
    // 确认弹窗覆盖在最上层（像素已在 showConfirmDialog 时缓存，关闭时还原）
    if (confirmDialog)
        renderConfirmDialog();
}

void MenuMode::renderConfigPanel()
{
    if (!scr) return;
    constexpr int panelW = 124;
    const int panelX = screenW - panelW;
    constexpr int scale = 1;

    // 右侧面板黑色背景
    scr->draw_retangle(panelX, 0, panelW, screenH, 0x000000);
    // 青色边框
    scr->draw_retangle(panelX, 0,                 panelW, 3,  0x00D4AA);
    scr->draw_retangle(panelX, screenH - 3,       panelW, 3,  0x00D4AA);
    scr->draw_retangle(panelX, 0,                 3,    screenH, 0x00D4AA);
    scr->draw_retangle(panelX + panelW - 3, 0,    3,    screenH, 0x00D4AA);

    // 标题
    Font::drawText(scr, panelX + 6, 4, "配置", 0x00D4AA, scale);

    // 三行配置：格子大小、起步速度、加速速度
    static const char* optNames[3] = { "格子大小", "起步速度", "加速速度" };
    static const int itemHeight = 158;
    static const char* cellLabels[3] = { "19+1小", "29+1中", "49+1大" };
    static const int*  speedOpts[3]  = { GameSpec::startSpeedOptions,
                                         GameSpec::boostSpeedOptions,
                                         nullptr };

    for (int i = 0; i < 3; ++i)
    {
        int rowY = 24 + i * itemHeight;

        // 每行背景条
        int rowBgColor = (static_cast<int>(selectedItem) == i + static_cast<int>(MenuItem::SINGLE_PLAYER))
                         ? 0x003355 : 0x101820;
        scr->draw_retangle(panelX + 4, rowY, panelW - 8, 148, rowBgColor);

        // 行标签
        Font::drawText(scr, panelX + 6, rowY + 4, optNames[i], 0xAAAAAA, scale);

        // 显示三个可选值，当前草稿选中项高亮（编辑期间只反映草稿）
        if (i == 0)  // 格子大小
        {
            for (int j = 0; j < 3; ++j)
            {
                int ox = panelX + 8 + j * 38;
                int oy = rowY + 26;
                bool sel = (j == draftCellSizeIndex);
                Font::drawText(scr, ox, oy, cellLabels[j],
                               sel ? 0xFFFF00 : 0x666666, scale);
                if (sel)
                    scr->draw_retangle(ox, oy + 12, 34, 2, 0xFFFF00);  // 下划线
            }
        }
        else  // 速度
        {
            const int* opts = (i == 1) ? GameSpec::startSpeedOptions
                                       : GameSpec::boostSpeedOptions;
            int idx = (i == 1) ? draftStartSpeedIndex : draftBoostSpeedIndex;
            for (int j = 0; j < 3; ++j)
            {
                char s[8];
                snprintf(s, sizeof(s), "%d", opts[j]);
                int ox = panelX + 8 + j * 38;
                int oy = rowY + 26;
                bool sel = (j == idx);
                Font::drawText(scr, ox, oy, s, sel ? 0xFFFF00 : 0x666666, scale);
                if (sel)
                    scr->draw_retangle(ox, oy + 12, 28, 2, 0xFFFF00);
            }
        }

        // 操作提示（← → 调整）
        const char* hint = "←→";
        Font::drawText(scr, panelX + panelW / 2 - Font::textWidth(hint, scale) / 2,
                       rowY + 134, hint, 0x444444, scale);
    }

    // 底部提示：回车确认 / ESC 退出
    const char* footer = "确认  ESC退出";
    Font::drawText(scr, panelX + panelW / 2 - Font::textWidth(footer, scale) / 2,
                   screenH - 20, footer, 0x444444, scale);
}

void MenuMode::renderMenuItems(bool highlightSelected)
{
    drawTitle();

    const int scale = 2;
    const int itemHeight = 56;
    const int startY = (screenH - static_cast<int>(MenuItem::COUNT) * itemHeight) / 2;

    for (int i = 0; i < static_cast<int>(MenuItem::COUNT); ++i)
    {
        bool selected = highlightSelected && (static_cast<MenuItem>(i) == selectedItem);
        bool disabled = (static_cast<MenuItem>(i) == MenuItem::CONTINUE_GAME && !hasSaveData);
        drawMenuItem(i, selected, disabled, startY + i * itemHeight);
    }
}

// ─────────────────────────────────────────────
// 工具函数
// ─────────────────────────────────────────────
void MenuMode::setSaveDataExists(bool exists)
{
    hasSaveData = exists;
    if (!exists && selectedItem == MenuItem::CONTINUE_GAME)
        selectedItem = MenuItem::SINGLE_PLAYER;
}

void MenuMode::setSingleMode(SingleMode* s)
{
    singleModePtr = s;
}

void MenuMode::drawMenuItem(int index, bool selected, bool disabled, int y)
{
    if (!scr) return;

    const char* labels[4] = { "单人模式", "双人模式", "继续游戏", "配置" };
    const char* label = labels[index];

    const int scale = 2;
    int textColor = disabled ? 0x666666 : (selected ? 0xFFFFFF : 0xAAAAAA);
    int bgColor   = selected ? 0x004488 : 0x101820;

    int textW = Font::textWidth(label, scale);
    int x = (screenW - textW) / 2;

    int boxPad = 16;
    scr->draw_retangle(x - boxPad, y - 6, textW + boxPad * 2, 16 * scale + 12, bgColor);

    if (selected && !disabled)
        scr->draw_retangle(x - boxPad - 14, y - 6, 8, 16 * scale + 12, 0xFFFF00);

    Font::drawText(scr, x, y, label, textColor, scale);
}

void MenuMode::drawTitle()
{
    if (!scr) return;
    const char* title = "贪吃蛇";
    const int scale = 3;
    int w = Font::textWidth(title, scale);
    Font::drawText(scr, (screenW - w) / 2, 60, title, 0x00D4AA, scale);
}

void MenuMode::clearScreen()
{
    if (scr) scr->clear(0x000000);
}

void MenuMode::clearScreenArea(int x, int y, int w, int h)
{
    if (!scr) return;
    scr->draw_retangle(x, y, w, h, 0x000000);
}
