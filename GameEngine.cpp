#include <chrono>
#include <unistd.h>
#include <algorithm>
#include <cstdio>
#include "GameEngine.h"
#include "Font.h"   // 暂停/结算文字绘制

GameEngine::GameEngine() :
    scr(Screen::get_screen()),
    lastTick(std::chrono::steady_clock::now()),
    menuMode(scr, false),
    twoPlayerMode(scr),
    pauseCache(),
    tickInterval(GameSpec::GAME_SPEED / GameSpec::getStartSpeed())
{
    if (scr)
    {
        screenW = scr->getWidth();
        screenH = scr->getHeight();
    }
    // 关联 SingleMode 给菜单（确认修改配置清档时使用），并同步初始存档状态
    menuMode.setSingleMode(&singleMode);
    menuMode.setSaveDataExists(singleMode.hasValidSave());
    // 默认进入主菜单，并立即绘制菜单（保证启动后第一页是菜单，而非对局）
    enterMenu();
}

GameEngine::~GameEngine()
{
}

void GameEngine::run()
{
    while (!gameOver)
    {
        processInput();        // 高频轮询输入并分发
        updateCurrentMode();   // 按节拍驱动当前模式

        usleep(10000);         // 10ms 让出 CPU
    }
}

// === 统一输入入口：按状态分发到独立处理器 ===
void GameEngine::processInput()
{
    const auto& keys = keyInput.pollKeys();

    for (Function key : keys)
    {
        if (key == Function::None) continue;

        switch (currentState)
        {
        case GameState::MENU:
            handleInput_Menu(key);
            break;
        case GameState::SINGLE_PLAYER:
            handleInput_SinglePlayer(key);
            break;
        case GameState::TWO_PLAYER:
            handleInput_TwoPlayer(key);
            break;
        case GameState::PAUSED:
            handleInput_Paused(key);
            break;
        case GameState::GAME_OVER:
            handleInput_GameOver(key);
            break;
        }
    }

    keyInput.clearQueue();
}

// === 菜单状态按键处理 ===
void GameEngine::handleInput_Menu(Function key)
{
    MenuItem action = menuMode.handleInput(key);

    if (action != MenuItem::COUNT)
    {
        switch (action)
        {
        case MenuItem::SINGLE_PLAYER:
            onMenuSelect_SinglePlayer();
            break;
        case MenuItem::TWO_PLAYER:
            onMenuSelect_TwoPlayer();
            break;
        case MenuItem::CONTINUE_GAME:
            onMenuSelect_ContinueGame();
            break;
        case MenuItem::CONFIG:
            onMenuSelect_Config();
            break;
        default:
            break;
        }
    }
    else
    {
        // 菜单内导航/其它按键：仅在此重绘菜单（解决屏闪问题）。
        // 静止时（无按键）不重绘，避免逐帧清屏闪烁。
        menuMode.render();
    }
}

// === 单人游戏按键处理 ===
void GameEngine::handleInput_SinglePlayer(Function key)
{
    switch (key)
    {
    case Function::Pause:
    case Function::MENU:
        // 暂停键 或 菜单键 -> 进入暂停/菜单
        if (key == Function::Pause)
        {
            enterPause();
        }
        else  // MENU 键
        {
            // 保存当前对局，回主菜单
            saveCurrentGame();
            prevGameState = currentState;
            enterMenu();
        }
        break;

    case Function::Quit:
        gameOver = true;
        break;

    case Function::Restart:
    case Function::Boost1:
        // R 键（产生 Boost1）：切换蛇速度（档位循环），立即刷新节拍间隔
        singleMode.boostSpeed();
        refreshTick(false);
        break;

    default:
        // 方向键等交给模式处理
        singleMode.handleInput(key);
        break;
    }
}

// === 双人游戏按键处理（预留）===
void GameEngine::handleInput_TwoPlayer(Function key)
{
    switch (key)
    {
    case Function::Pause:
    case Function::MENU:
        if (key == Function::Pause)
        {
            enterPause();
        }
        else
        {
            saveCurrentGame();
            prevGameState = currentState;
            twoPlayerMode.stop();   // 停止双人蛇线程（本分支不走 enterMenu）
            currentState = GameState::MENU;
            menuMode.setSaveDataExists(false);  // 双人模式暂不支持存档
            menuMode.render();
        }
        break;
    case Function::Quit:
        gameOver = true;
        break;
    default:
        twoPlayerMode.handleInput(key);
        break;
    }
}

// === 暂停状态按键处理 ===
void GameEngine::handleInput_Paused(Function key)
{
    switch (key)
    {
    case Function::Pause:
    case Function::Resume:
        // 恢复游戏
        exitPause();
        break;
    case Function::MENU:
        // 保存并回主菜单
        saveCurrentGame();
        exitPause();  // 先还原画面
        enterMenu();
        break;
    case Function::Quit:
        gameOver = true;
        break;
    default:
        break;
    }
}

// === 游戏结束按键处理 ===
void GameEngine::handleInput_GameOver(Function key)
{
    switch (key)
    {
    case Function::MENU:
    case Function::Confirm:
    case Function::Resume:
        // 回主菜单
        enterMenu();
        break;
    case Function::Quit:
        gameOver = true;
        break;
    default:
        break;
    }
}

// === 按节拍驱动当前模式 ===
void GameEngine::updateCurrentMode()
{
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTick).count();

    if (elapsed < tickInterval)
        return;

    lastTick = now;

    switch (currentState)
    {
    case GameState::SINGLE_PLAYER:
        singleMode.update();
        singleMode.render();
        if (singleMode.isGameOver())
        {
            // 进入 GAME_OVER 状态并绘制结算画面
            currentState = GameState::GAME_OVER;
            drawGameOver();
        }
        else
        {
            refreshTick(false);
        }
        break;

    case GameState::TWO_PLAYER:
        twoPlayerMode.update();
        twoPlayerMode.render();
        if (twoPlayerMode.isGameOver())
        {
            // 进入 GAME_OVER 状态。结算画面已由 TwoPlayerMode::render() 在最后一帧绘制，
            // GAME_OVER 状态只处理输入、不重绘，保持结算画面。
            currentState = GameState::GAME_OVER;
        }
        break;

    case GameState::MENU:
        // 菜单为静态画面，仅在“进入菜单”和“按键”时重绘，
        // 这里不再逐帧重绘，避免屏闪。
        break;

    case GameState::PAUSED:
        // 暂停状态不更新游戏逻辑，暂停遮罩已在 enterPause 绘制
        break;

    case GameState::GAME_OVER:
        // 游戏结束画面已在进入该状态时绘制一次，这里无需重复重绘
        break;
    }
}

// === 菜单动作实现 ===
void GameEngine::enterMenu()
{
    twoPlayerMode.stop();   // 离开对局：确保双人蛇线程已停止（首次/已停时为空操作）
    currentState = GameState::MENU;
    menuMode.setSaveDataExists(singleMode.hasValidSave());
    menuMode.render();   // 进入菜单时绘制一次（菜单为静态画面，无需逐帧重绘）
}

// 根据速度刷新节拍间隔：集中此处，避免多处重复且易漏改（如新增状态/速度模型）。
void GameEngine::refreshTick(bool useStartSpeed)
{
    const int speed = useStartSpeed ? GameSpec::getStartSpeed() : singleMode.getSnakeSpeed();
    tickInterval = GameSpec::GAME_SPEED / speed;
}

void GameEngine::onMenuSelect_SinglePlayer()
{
    twoPlayerMode.stop();   // 若此前在双人模式，确保其蛇线程已停止
    // 开启新单人对局：重置并进入游戏
    singleMode.resetGame();
    currentState = GameState::SINGLE_PLAYER;
    refreshTick(true);
    lastTick = std::chrono::steady_clock::now();
    singleMode.render_full();   // 首帧：完整清屏+画棋盘，避免残留菜单画面
}

void GameEngine::onMenuSelect_TwoPlayer()
{
    twoPlayerMode.resetGame();   // 重新开局（内部会先停旧线程，再启动两条蛇的移动线程）
    currentState = GameState::TWO_PLAYER;
    // 草地刷新的帧率：每 10ms 监视一次两条蛇的头/食物是否变化并增量重绘。
    // 蛇的“移动快慢”由各蛇自己的移动线程按时间累积决定（加速就快），
    // 与草地刷新频率解耦——移动不移动是蛇自己的事，草地只负责监视变化。
    tickInterval = 10;
    lastTick = std::chrono::steady_clock::now();
    twoPlayerMode.render_full();   // 首帧：立即清屏重绘，避免残留菜单画面/“卡死”假象
}

void GameEngine::onMenuSelect_ContinueGame()
{
    // 继续游戏：加载存档
    if (loadSavedGame())
    {
        currentState = GameState::SINGLE_PLAYER;
        refreshTick(false);
        lastTick = std::chrono::steady_clock::now();
        singleMode.render_full();   // 首帧：完整清屏+画棋盘，避免残留画面
    }
}

void GameEngine::onMenuSelect_Config()
{
    // 格子大小变化 → 重新创建 Board；否则只需刷新 tickInterval
    bool cellSizeChanged = (currentState == GameState::SINGLE_PLAYER ||
                            currentState == GameState::PAUSED);
    if (cellSizeChanged)
        singleMode.recalcBoard();
    // 用配置的起始速度重置 tickInterval（而非当前可能已被 R 加速的速度）
    if (currentState == GameState::SINGLE_PLAYER)
        refreshTick(true);
    // 回到主菜单，重绘菜单（配置面板关闭）
    enterMenu();
}

void GameEngine::drawGameOver()
{
    if (!scr) return;
    scr->clear(0x000000);

    const int scale = 3;
    const char* title = singleMode.isWin() ? "胜利" : "游戏结束";
    int titleColor = singleMode.isWin() ? 0x40FF40 : 0xFF4040;
    int w = Font::textWidth(title, scale);
    Font::drawText(scr, (screenW - w) / 2, screenH / 2 - 40, title, titleColor, scale);

    // 得分（英文，避免多余中文字模依赖）
    char score[32];
    snprintf(score, sizeof(score), "SCORE: %d", singleMode.getScore());
    int sw = Font::textWidth(score, 2);
    Font::drawText(scr, (screenW - sw) / 2, screenH / 2 + 20, score, 0xFFFFFF, 2);

    // 提示返回菜单
    const char* hint = "PRESS MENU";
    int hw = Font::textWidth(hint, 2);
    Font::drawText(scr, (screenW - hw) / 2, screenH / 2 + 60, hint, 0xAAAAAA, 2);
}

// === 存档管理 ===
void GameEngine::saveCurrentGame()
{
    if (currentState == GameState::SINGLE_PLAYER || currentState == GameState::PAUSED)
    {
        singleMode.saveState();
        menuMode.setSaveDataExists(true);
    }
}

bool GameEngine::loadSavedGame()
{
    if (singleMode.hasValidSave())
    {
        singleMode.loadState();
        return true;
    }
    return false;
}

// === 暂停机制：像素缓存还原 ===
void GameEngine::enterPause()
{
    if (currentState != GameState::SINGLE_PLAYER && currentState != GameState::TWO_PLAYER)
        return;

    prevGameState = currentState;
    currentState = GameState::PAUSED;

    // 双人模式：冻结蛇移动线程（不退出进程），恢复时继续。这样暂停期间蛇不会移动，
    // 恢复时屏幕与蛇位置一致，不会再残留整条旧蛇。单人模式移动由节拍驱动，无需此步。
    if (prevGameState == GameState::TWO_PLAYER)
        twoPlayerMode.setPaused(true);

    // 1. 缓存暂停遮罩区域原始像素
    cachePauseArea();

    // 2. 绘制半透明暂停遮罩
    drawPauseOverlay();
}

void GameEngine::exitPause()
{
    if (currentState != GameState::PAUSED)
        return;

    // 1. 还原缓存像素（避免全屏重绘）
    restorePauseArea();

    // 2. 恢复之前的游戏状态
    currentState = prevGameState;
    lastTick = std::chrono::steady_clock::now();  // 重置节拍防止瞬间更新

    // 双人模式：恢复蛇移动线程（与 enterPause 的 setPaused(true) 配对）。
    // 暂停期间线程已冻结、蛇未移动，恢复后屏幕与蛇位置一致，无残留。
    if (prevGameState == GameState::TWO_PLAYER)
        twoPlayerMode.setPaused(false);
}

void GameEngine::cachePauseArea()
{
    if (!scr) return;

    // 暂停遮罩区域：屏幕中心矩形
    int overlayW = screenW / 2;
    int overlayH = screenH / 3;
    int overlayX = (screenW - overlayW) / 2;
    int overlayY = (screenH - overlayH) / 2;

    pauseCache.x = overlayX;
    pauseCache.y = overlayY;
    pauseCache.w = overlayW;
    pauseCache.h = overlayH;

    pauseCache.pixels.resize(overlayW * overlayH);

    int* fb = scr->getFrameBuffer();

    // 从帧缓冲区读取像素
    for (int y = 0; y < overlayH; ++y)
    {
        for (int x = 0; x < overlayW; ++x)
        {
            int srcX = overlayX + x;
            int srcY = overlayY + y;
            if (srcX >= 0 && srcX < screenW && srcY >= 0 && srcY < screenH)
            {
                pauseCache.pixels[y * overlayW + x] = fb[srcY * screenW + srcX];
            }
        }
    }
    pauseCache.valid = true;
}

void GameEngine::restorePauseArea()
{
    if (!scr || !pauseCache.valid) return;

    int overlayW = pauseCache.w;
    int overlayH = pauseCache.h;
    int overlayX = pauseCache.x;
    int overlayY = pauseCache.y;

    int* fb = scr->getFrameBuffer();

    // 直接写回缓存像素
    for (int y = 0; y < overlayH; ++y)
    {
        for (int x = 0; x < overlayW; ++x)
        {
            int dstX = overlayX + x;
            int dstY = overlayY + y;
            if (dstX >= 0 && dstX < screenW && dstY >= 0 && dstY < screenH)
            {
                fb[dstY * screenW + dstX] = pauseCache.pixels[y * overlayW + x];
            }
        }
    }

    pauseCache.valid = false;  // 标记缓存已用，下次重新缓存
}

void GameEngine::drawPauseOverlay()
{
    if (!scr) return;

    int overlayW = screenW / 2;
    int overlayH = screenH / 3;
    int overlayX = (screenW - overlayW) / 2;
    int overlayY = (screenH - overlayH) / 2;

    // 暂停提示矩形颜色（半透明背景 + 亮色提示框）
    const int overlayColor = 0x000000;  // 遮罩底色（深色）
    const int hintColor   = 0xFFFF00;   // 提示框（脉冲黄）

    // 半透明叠加：在已缓存的原始像素上按 alpha 混合遮罩底色，
    // 还原阶段直接写回缓存像素即可恢复原画面，无需全屏重绘。
    int* fb = scr->getFrameBuffer();
    const int alpha = 160;  // 0~255，越大越不透明
    for (int y = 0; y < overlayH; ++y)
    {
        for (int x = 0; x < overlayW; ++x)
        {
            int dstX = overlayX + x;
            int dstY = overlayY + y;
            if (dstX < 0 || dstX >= screenW || dstY < 0 || dstY >= screenH)
                continue;

            int base = fb[dstY * screenW + dstX];
            // 分通道混合：out = base*(1-a) + overlay*a
            int br = (base & 0xFF);
            int bg = (base >> 8) & 0xFF;
            int bb = (base >> 16) & 0xFF;
            int or_ = (overlayColor & 0xFF);
            int og = (overlayColor >> 8) & 0xFF;
            int ob = (overlayColor >> 16) & 0xFF;
            int r = (br * (255 - alpha) + or_ * alpha) / 255;
            int g = (bg * (255 - alpha) + og * alpha) / 255;
            int b = (bb * (255 - alpha) + ob * alpha) / 255;
            fb[dstY * screenW + dstX] = (r) | (g << 8) | (b << 16);
        }
    }

    // 屏幕中心绘制提示框（亮色边框矩形），标明“已暂停”
    int boxW = overlayW - 40;
    int boxH = 60;
    int boxX = overlayX + 20;
    int boxY = overlayY + (overlayH - boxH) / 2;
    scr->draw_retangle(boxX, boxY, boxW, boxH, hintColor);

    // 提示文字：中文“暂停” + 英文“PAUSE”，以及当前得分
    const int txtScale = 2;
    const char* cn = "暂停";
    const char* en = "PAUSE";
    int cnW = Font::textWidth(cn, txtScale);
    Font::drawText(scr, overlayX + (overlayW - cnW) / 2, boxY + 6, cn, 0x000000, txtScale);
    int enW = Font::textWidth(en, 1);
    Font::drawText(scr, overlayX + (overlayW - enW) / 2, boxY + 32, en, 0x000000, 1);

    // 得分（英文，避免依赖未收录的中文字模）
    char score[24];
    snprintf(score, sizeof(score), "SCORE:%d", singleMode.getScore());
    int scW = Font::textWidth(score, 1);
    Font::drawText(scr, overlayX + (overlayW - scW) / 2, boxY + boxH + 8, score, 0xFFFF00, 1);
}