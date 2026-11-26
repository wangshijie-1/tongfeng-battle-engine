#include "TwoPlayerMode.h"
#include "World_Unit.h"
#include "Font.h"
#include <cstdio>
#include <cstring>     // memcpy：准备阶段提示文字的像素记录/还原

TwoPlayerMode::TwoPlayerMode(Screen* s) :
    screen(s ? s : Screen::get_screen()),
    board(),
    food(Position{ 1,1 }, GameSpec::getCellSize(), Color[FOOD])
{
    initGame();
}

TwoPlayerMode::~TwoPlayerMode()
{
    stopThreads();
}

// ===================== 初始化 =====================
void TwoPlayerMode::initGame()
{
    int screenH = screen ? screen->getHeight() : GameSpec::SCREEN_H_DEFAULT;
    int screenW = screen ? screen->getWidth()  : GameSpec::BOARD_RENDER_W;
    board = Board(GameSpec::getGridW(screenW), GameSpec::getGridH(screenH));

    // 建墙（照抄单人）
    for (int i = 0; i < board.getWidth(); i++)
    {
        board.setCellType({ i, 0 }, CellType::WALL);
        board.setCellType({ i, board.getHeight() - 1 }, CellType::WALL);
    }
    for (int i = 0; i < board.getHeight(); i++)
    {
        board.setCellType({ 0, i }, CellType::WALL);
        board.setCellType({ board.getWidth() - 1, i }, CellType::WALL);
    }

    int w = board.getWidth();
    int h = board.getHeight();

    // 两条蛇对角出生，保持与墙的距离（出生不会撞墙）
    Position head1 = { w / 4,     h - 3 };   // P1(WASD)：左下，朝右
    Position head2 = { 3 * w / 4, 2 };        // P2(方向键)：右上，朝左

    // 颜色区分：P1 火山红头 + 全息青身；P2 阳光黄头 + 橙黄身
    snake1.setColors(0xFF0000, 0x00D4AA);
    snake2.setColors(0xFFC400, 0xFF8000);

    snake1.initAt(head1, Direction::RIGHT, 3);
    snake2.initAt(head2, Direction::LEFT,  3);

    dead1.store(false);
    dead2.store(false);
    gameOver.store(false);
    winner.store(0);
    started.store(false);   // 准备阶段：等任一方按方向键才开始
    hintActive = false;      // 准备阶段提示：下一帧重新记录原像素并绘制

    // 食物（照抄单人）
    food.setExist(false);
    spawnFood();

    // 把初始蛇身写进棋盘标记（供 spawnFood 避让查询；渲染走 diff 不依赖它）。
    // 注意：initAt 只设置 body，不设置 new_head，故此处必须用 body 真实坐标，
    // 绝不能碰未初始化的 new_head（否则把垃圾坐标写进 board，棋盘会画出随机色块，
    // 还可能越界破坏内存，导致静止时蛇头渲染异常）。
    for (const auto& p : snake1.getBody()) board.setCellType(p, CellType::SNAKE_BODY);
    board.setCellType(snake1.getBody().front(), CellType::SNAKE_HEAD);
    for (const auto& p : snake2.getBody()) board.setCellType(p, CellType::SNAKE_BODY);
    board.setCellType(snake2.getBody().front(), CellType::SNAKE_HEAD);
    if (food.getExist())
        board.setCellType(food.getPos(), CellType::FOOD);
}

// ===================== 食物生成（照抄单人） =====================
void TwoPlayerMode::spawnFood()
{
    if (food.getExist()) return;
    int max = 1000;
    while (max--)
    {
        food.update(board.getWidth(), board.getHeight());
        if (board.isValidPosition(food.getPos()) &&
            board.getCellType(food.getPos()) == CellType::EMPTY)
        {
            board.setCellType(food.getPos(), CellType::FOOD);
            food.setExist(true);
            return;
        }
    }
    // 兜底：顺序找一个空格
    for (int x = 1; x < board.getWidth() - 1; x++)
        for (int y = 1; y < board.getHeight() - 1; y++)
        {
            Position p{ x, y };
            if (board.getCellType(p) == CellType::EMPTY)
            {
                food.setPos(p);
                board.setCellType(p, CellType::FOOD);
                food.setExist(true);
                return;
            }
        }
}

// ===================== 输入 =====================
void TwoPlayerMode::handleInput(Function key)
{
    switch (key)
    {
    // ---- P1: WASD ----
    case Function::Up:    snake1.changeDirection(Direction::UP);    started.store(true); break;
    case Function::Down:  snake1.changeDirection(Direction::DOWN);  started.store(true); break;
    case Function::Left:  snake1.changeDirection(Direction::LEFT);  started.store(true); break;
    case Function::Right: snake1.changeDirection(Direction::RIGHT); started.store(true); break;
    case Function::Boost1:
        snake1.toggleSpeed();   // P1 独立加速
        break;

    // ---- P2: 方向键 ----
    case Function::UpTwo:    snake2.changeDirection(Direction::UP);    started.store(true); break;
    case Function::DownTwo:  snake2.changeDirection(Direction::DOWN);  started.store(true); break;
    case Function::LeftTwo:  snake2.changeDirection(Direction::LEFT);  started.store(true); break;
    case Function::RightTwo: snake2.changeDirection(Direction::RIGHT); started.store(true); break;
    case Function::Boost2:
        snake2.toggleSpeed();   // P2 独立加速
        break;

    default:
        break;
    }
}

// ===================== 碰撞判定（撞墙/自撞照抄单人 + 撞对方身） =====================
// 调用时机：蛇已 move() 之后，get_new_head() 是真实移动后的头（与单人范式一致）。
bool TwoPlayerMode::checkCollision(Snake* self, Snake* other)
{
    Position head = self->get_new_head();

    // 撞墙 / 越界（照抄单人）
    if (head.x < 1 || head.x > board.getWidth() - 2 ||
        head.y < 1 || head.y > board.getHeight() - 2)
        return true;

    // 自撞（照抄单人：从 body[1] 开始，跳过本步将移走的旧尾以免误判）
    const auto& body = self->getBody();
    for (size_t i = 1; i + 1 < body.size(); ++i)
        if (head == body[i]) return true;

    // 撞对方身体（含对方头）：无论碰到谁的身体都死
    const auto& ob = other->getBody();
    for (size_t i = 0; i < ob.size(); ++i)
        if (head == ob[i]) return true;

    return false;
}

// ===================== 结算 =====================
void TwoPlayerMode::finalize()
{
    if (gameOver.load()) return;   // 只结算一次
    if (dead1.load() && dead2.load())
    {
        // 同时死亡：比长度，长者为胜，等长平局
        int l1 = snake1.getLength();
        int l2 = snake2.getLength();
        winner.store((l1 > l2) ? 1 : (l2 > l1) ? 2 : 0);
    }
    else if (dead1.load())
        winner.store(2);   // 一方死亡，对方赢
    else if (dead2.load())
        winner.store(1);
    gameOver.store(true);
}

// ===================== 单条蛇的移动线程 =====================
// 完全照搬单人范式：每个节拍蛇 move() 一次，move 之后立即判碰撞/食物。
// 不同点：每步由“累计时间 >= 自己的 interval”触发，interval 由 speed 决定，
// 因此两条蛇可独立不同速（加速就快）；移动在锁内完成，渲染线程不会读到半更新状态。
void TwoPlayerMode::snakeThread(Snake* self, Snake* other,
                                std::atomic<bool>* selfDead,
                                std::atomic<bool>* otherDead)
{
    int acc = 0;
    while (running.load())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(FRAME_MS));

        if (paused.load()) { acc = 0; continue; }   // 暂停：冻结线程，不累计不移动

        if (!started.load()) { acc = 0; continue; }   // 准备阶段不移动
        if (selfDead->load()) continue;               // 已死的蛇不再动

        acc += FRAME_MS;
        int interval = GameSpec::GAME_SPEED / self->getSpeed();   // 普通350ms / 加速200ms
        if (acc >= interval)
        {
            acc -= interval;
            std::lock_guard<std::mutex> lock(mtx);
            if (selfDead->load()) continue;

            self->move();   // 移动是蛇自己的事（照抄单人）

            // 死亡判定：仅在本步真实移动之后进行（绝不预判）
            if (checkCollision(self, other))
            {
                self->setdead();
                selfDead->store(true);
                finalize();
            }
            else if (self->get_new_head() == other->get_new_head())
            {
                // 两蛇头相撞 → 都死
                self->setdead();
                other->setdead();
                selfDead->store(true);
                otherDead->store(true);
                finalize();
            }
            else
            {
                // 食物：吃到 → 增长 + 重新生成（照抄单人逻辑）
                if (self->get_new_head() == food.getPos())
                {
                    self->setgrowing();
                    food.setExist(false);
                    spawnFood();
                }
                // 维护棋盘标记（照抄单人：擦旧尾、写蛇身、写头、写食物）
                for (const auto& p : self->getBody()) board.setCellType(p, CellType::SNAKE_BODY);
                board.setCellType(self->get_new_head(), CellType::SNAKE_HEAD);
                board.setCellType(self->getOldTail(), CellType::EMPTY);
                if (food.getExist()) board.setCellType(food.getPos(), CellType::FOOD);
            }
        }
    }
}

// ===================== 线程管理 =====================
void TwoPlayerMode::stopThreads()
{
    running.store(false);
    if (thread1.joinable()) thread1.join();
    if (thread2.joinable()) thread2.join();
}

void TwoPlayerMode::stop()
{
    stopThreads();
}

void TwoPlayerMode::update()
{
    // 移动由各蛇线程完成；引擎节拍里无需做任何事。
}

// ===================== 渲染（草地只监视变化） =====================
// 核心：不整盘重绘、不遍历整条蛇。每帧只比较“蛇头/食物位置有没有变化”，
// 变了才画新头+擦旧尾，没变就什么都不动。移动与否是蛇自己的事，草地只负责监视。
// BOARD_X0/BOARD_Y0 已在 render.h 中定义为全局 constexpr，这里直接使用。

// 照抄单人成熟增量渲染（render_snake_food 逻辑），每帧无条件执行：
//   1) 若非增长，擦旧蛇尾(EMPTY)；
//   2) 画蛇头(get_new_head，专属头色)；
//   3) 画蛇头后一节(body[1]，专属身色)。
// 依赖 Snake::get_new_head()/getOldTail()/isGrowing()，initAt 已置这两个值为真实坐标。
void TwoPlayerMode::drawSnakeMature(Snake* self, int headColor, int bodyColor)
{
    if (!screen || !self) return;
    const int step = GameSpec::getCellStep();
    const int size = GameSpec::getCellSize();

    std::lock_guard<std::mutex> lock(mtx);

    // 1) 擦旧蛇尾（增长那帧不擦，与单人逻辑一致）
    if (!self->isGrowing())
    {
        screen->draw_retangle(BOARD_X0 + step * self->getOldTail().x,
                              BOARD_Y0 + step * self->getOldTail().y,
                              size, size, Color[EMPTY]);
    }

    // 2) 画蛇头（专属头色）
    const Position& head = self->get_new_head();
    screen->draw_retangle(BOARD_X0 + step * head.x,
                          BOARD_Y0 + step * head.y,
                          size, size, headColor);

    // 3) 画蛇头后一节（专属身色）
    if (self->getBody().size() > 1)
    {
        const Position& nextHead = self->getBody()[1];
        screen->draw_retangle(BOARD_X0 + step * nextHead.x,
                              BOARD_Y0 + step * nextHead.y,
                              size, size, bodyColor);
    }
}

void TwoPlayerMode::render_full()
{
    if (!screen) return;
    int s1, s2;
    {
        std::lock_guard<std::mutex> lock(mtx);
        // 整盘清屏 + 画墙/食物/蛇1（只调用一次）
        renderer.render(board, snake1, food.getPos(), screen);
        // 首帧把蛇2整条按专属色画上（render 只画了 snake1）
        renderer.renderSnake(board, snake2, screen);

        // 记录本帧食物位置，供 render() 的“擦除上一帧食物”逻辑做基准
        lastFoodPos = food.getPos();

        // 锁内读取体力初值，避免与蛇线程 move() 中的体力写入产生数据竞争
        s1 = snake1.getStamina();
        s2 = snake2.getStamina();
    }

    // 右侧栏：初始化一次（黑底 + 青色描边 + P1/P2 标签 + 长度数字）
    const int panelW = GameSpec::PANEL_W;
    const int panelX = screen->getWidth() - panelW;
    const int panelH = screen->getHeight();
    screen->draw_retangle(panelX, 0, panelW, panelH, 0x000000);
    screen->draw_retangle(panelX, 0, panelW, 4, 0x00D4AA);
    screen->draw_retangle(panelX, panelH - 4, panelW, 4, 0x00D4AA);
    screen->draw_retangle(panelX, 0, 4, panelH, 0x00D4AA);
    screen->draw_retangle(panelX + panelW - 4, 0, 4, panelH, 0x00D4AA);
    Font::drawText(screen, panelX + 16, 24, "P1", snake1.getBodyColor(), 2);
    Font::drawText(screen, panelX + 16, panelH / 2, "P2", snake2.getBodyColor(), 2);
    // 体力区：P1/P2 各自长度数字下方（标签 STA + 当前/上限，居中绘制）
    renderer.renderStaminaPanel(screen, 84, Snake::MAX_STAMINA, s1);
    renderer.renderStaminaPanel(screen, 360, Snake::MAX_STAMINA, s2);
    drawSideNumbers();
}

// 右侧栏：只更新长度数字与体力数值（背景/标签已存在，不重绘，避免覆盖游戏画面）
void TwoPlayerMode::drawSideNumbers()
{
    if (!screen) return;
    int l1, l2, s1, s2;
    {
        std::lock_guard<std::mutex> lock(mtx);
        l1 = snake1.getLength();
        l2 = snake2.getLength();
        s1 = snake1.getStamina();
        s2 = snake2.getStamina();
    }
    const int panelW = GameSpec::PANEL_W;
    const int panelX = screen->getWidth() - panelW;
    const int scale = 1;
    const int numH = 16 * scale;

    // P1 长度
    int y1 = 24 + 16 * 2 + 8;
    screen->draw_retangle(panelX + 4, y1, panelW - 8, numH, 0x000000);
    char b1[16]; snprintf(b1, sizeof(b1), "LEN:%d", l1);
    Font::drawText(screen, panelX + 16, y1, b1, 0xFFFFFF, scale);

    // P2 长度
    int y2 = screen->getHeight() / 2 + 16 * 2 + 8;
    screen->draw_retangle(panelX + 4, y2, panelW - 8, numH, 0x000000);
    char b2[16]; snprintf(b2, sizeof(b2), "LEN:%d", l2);
    Font::drawText(screen, panelX + 16, y2, b2, 0xFFFFFF, scale);

    // 体力数值（标签 STA 在 render_full 已画）：P1 在长度下方、P2 在长度下方
    renderer.renderStaminaOnly(screen, 96, Snake::MAX_STAMINA, s1);
    renderer.renderStaminaOnly(screen, 372, Snake::MAX_STAMINA, s2);
}

// 准备阶段提示：进入时“先记录覆盖区原像素，再画一次文字”；开始后由 clearStartHint 一次性还原
void TwoPlayerMode::showStartHint()
{
    if (!screen || hintActive) return;   // 只记录/画一次
    const char* tip = "PRESS WASD / ARROWS TO START";
    const int scale = 2;
    int tw = Font::textWidth(tip, scale);
    int tx = (screen->getWidth() - tw) / 2;
    int ty = screen->getHeight() / 2;
    hintX = tx - 4; hintY = ty - 4; hintW = tw + 8; hintH = 16 * scale + 8;

    int* fb = screen->getFrameBuffer();
    int W  = screen->getWidth();
    hintBuf.assign((size_t)hintW * hintH, 0);
    for (int r = 0; r < hintH; ++r)
    {
        int yy = hintY + r;
        if (yy < 0 || yy >= screen->getHeight()) continue;
        const int* src = fb + (size_t)yy * W + hintX;
        memcpy(&hintBuf[(size_t)r * hintW], src, (size_t)hintW * sizeof(int));
    }
    Font::drawText(screen, tx, ty, tip, 0xFFFFFF, scale);
    hintActive = true;
}

// 玩家开始运行（按方向键）时调用：把之前记录的原始像素写回，文字即被擦除（只调用一次）
void TwoPlayerMode::clearStartHint()
{
    if (!screen || !hintActive) return;
    int* fb = screen->getFrameBuffer();
    int W  = screen->getWidth();
    for (int r = 0; r < hintH; ++r)
    {
        int yy = hintY + r;
        if (yy < 0 || yy >= screen->getHeight()) continue;
        int* dst = fb + (size_t)yy * W + hintX;
        memcpy(dst, &hintBuf[(size_t)r * hintW], (size_t)hintW * sizeof(int));
    }
    hintActive = false;
}

void TwoPlayerMode::render()
{
    if (!screen) return;

    // ===== 照抄单人成熟渲染（SingleMode::render + render_snake_food）=====
    // 关键：吃食物后该格不再标记 FOOD，但屏幕上那块黄色像素不会被任何逻辑清除
    // （增量渲染只擦蛇尾），会一直残留，造成“食物还在原地”的错觉。
    // 先按 EMPTY 擦掉上一帧食物位置，再画当前食物。
    const int step = GameSpec::getCellStep();
    const int size = GameSpec::getCellSize();
    Position foodPos; bool foodExist;
    {
        std::lock_guard<std::mutex> lock(mtx);
        foodPos = food.getPos();
        foodExist = food.getExist();
    }
    if (lastFoodPos.x >= 0 && lastFoodPos.x < board.getWidth() &&
        lastFoodPos.y >= 0 && lastFoodPos.y < board.getHeight())
    {
        screen->draw_retangle(BOARD_X0 + step * lastFoodPos.x,
                              BOARD_Y0 + step * lastFoodPos.y,
                              size, size, Color[EMPTY]);
    }
    // 画当前食物（共享一个；若本帧刚吃掉，food 已 respawn 到新位置）
    if (foodExist)
        screen->draw_retangle(BOARD_X0 + step * foodPos.x,
                              BOARD_Y0 + step * foodPos.y,
                              size, size, Color[FOOD]);
    lastFoodPos = foodPos;

    // 两条蛇各自用成熟增量渲染：无条件擦旧尾+画头+画头后一节（各用专属颜色）
    drawSnakeMature(&snake1, snake1.getHeadColor(), snake1.getBodyColor());
    drawSnakeMature(&snake2, snake2.getHeadColor(), snake2.getBodyColor());

    // 右侧栏只更新长度数字
    drawSideNumbers();

    // 游戏结束结算（增量叠加，不整盘清，避免闪烁）
    if (gameOver.load())
    {
        const int scale = 3;
        const char* title = "游戏结束";
        int w = Font::textWidth(title, scale);
        Font::drawText(screen, (screen->getWidth() - w) / 2, screen->getHeight() / 2 - 60,
                       title, 0xFF4040, scale);

        char result[32];
        if (winner.load() == 0)
            snprintf(result, sizeof(result), "DRAW");
        else
            snprintf(result, sizeof(result), "玩家%d 赢", winner.load());

        int rw = Font::textWidth(result, 2);
        Font::drawText(screen, (screen->getWidth() - rw) / 2, screen->getHeight() / 2,
                       result, 0xFFFFFF, 2);

        const char* hint = "PRESS MENU";
        int hw = Font::textWidth(hint, 2);
        Font::drawText(screen, (screen->getWidth() - hw) / 2, screen->getHeight() / 2 + 60,
                       hint, 0xAAAAAA, 2);
    }

    // 准备阶段提示：进入时记录原像素并画一次；开始后一次性还原（不每帧重绘）
    if (started.load())
    {
        if (hintActive) clearStartHint();
    }
    else if (!gameOver.load())
    {
        showStartHint();
    }
}

// ===================== 重置 =====================
void TwoPlayerMode::resetGame()
{
    stopThreads();      // 先停旧线程（首次调用线程未启动，joinable() 为 false，安全）
    initGame();         // 重建棋盘与两蛇初始状态
    paused.store(false);   // 清零暂停标记，避免上次暂停残留导致新开局直接冻结
    running.store(true);
    thread1 = std::thread(&TwoPlayerMode::snakeThread, this, &snake1, &snake2, &dead1, &dead2);
    thread2 = std::thread(&TwoPlayerMode::snakeThread, this, &snake2, &snake1, &dead2, &dead1);
}

// static const 成员的“类外定义”，满足 ODR（被 sleep_for(milliseconds(FRAME_MS)) 取引用）
const int TwoPlayerMode::FRAME_MS;
