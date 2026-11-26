#include "SingleMode.h"
#include <vector>
#include <cstdio>

// 历史最高分（跨局持久化，编译期零值）
static int g_highScore = 0;

SingleMode::SingleMode() :
    board(GameSpec::getGridW(Screen::get_screen()->getWidth()),
          GameSpec::getGridH(Screen::get_screen()->getHeight())),
    food({0, 0}, GameSpec::getCellSize(), Color[FOOD]),
    scr(nullptr),
    hasSave(false)
{
    scr = Screen::get_screen();
    initGame();
}

SingleMode::~SingleMode()
{
}

void SingleMode::handleInput(Function key)
{
    Direction dir = Direction::NONE;
    switch (key)
    {
    case Function::Up:
    case Function::UpTwo:
        dir = Direction::UP;
        break;
    case Function::Down:
    case Function::DownTwo:
        dir = Direction::DOWN;
        break;
    case Function::Left:
    case Function::LeftTwo:
        dir = Direction::LEFT;
        break;
    case Function::Right:
    case Function::RightTwo:
        dir = Direction::RIGHT;
        break;
    default:
        break;
    }

    if (dir != Direction::NONE)
    {
        snake.changeDirection(dir);
    }
}

void SingleMode::update()
{
    if (gameOver)
        return;

    // 食物每 20 秒自动消失并重新随机生成：
    // 若当前食物存在且已存活超过 FOOD_LIFETIME_MS，则先将其从棋盘擦除并置为不存在，
    // 再让 spawnFood() 重新随机生成一个新食物，同时刷新食物生成时间。
    if (food.getExist())
    {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - foodSpawnTime).count();
        if (elapsed >= FOOD_LIFETIME_MS)
        {
            board.setCellType(food.getPos(), CellType::EMPTY);
            food.setExist(false);
            spawnFood();
        }
    }

    snake.move();

    // 碰撞检测必须在操作 Board 之前，否则越界坐标会访问非法内存
    if (checkCollision())
    {
        snake.setdead();
        gameOver = true;
        return;
    }

    spawnFood();

    if (snake.get_new_head() == food.getPos())
    {
        snake.setgrowing();
        food.setExist(false);
        // 分数 = 蛇身长度。注意：snake.move() 已在本帧执行，而 setgrowing() 只在
        // 【下一次】move() 时才真正 +1，因此此处 getLength() 仍是吃果前的长度，
        // 直接使用会让“当前得分”和“历史最高分”都少 1 分（吃第一个果子不涨分、
        // 最高分永远比正确值少一分）。吃到食物必定使长度 +1，故显示 getLength()+1。
        int cur = snake.getLength() + 1;
        updateHighScore(cur);
        renderer.renderScoreOnly(scr, cur, g_highScore);
    }

    for (auto it = snake.getBody().begin(); it != snake.getBody().end(); ++it)
    {
        board.setCellType(*it, CellType::SNAKE_BODY);
    }
    board.setCellType(snake.get_new_head(), CellType::SNAKE_HEAD);
    board.setCellType(snake.getOldTail(), CellType::EMPTY);
    // 仅当食物仍存在（本帧未被吃）时才把该格标记为 FOOD；
    // 若本帧刚吃掉，food.getPos() 仍指向旧食物格，不应再标记为 FOOD。
    if (food.getExist())
        board.setCellType(food.getPos(), CellType::FOOD);

    // 胜利检测（单人模式）：蛇身填满所有有效草地格即获胜
    if (snake.getLength() >= freeCells)
    {
        won = true;
        gameOver = true;
        updateHighScore(getScore());
    }
}

void SingleMode::render_full()
{
    renderer.render(board, snake, food.getPos(), scr);
    // 首帧完成后绘制右侧分数侧栏
    renderer.renderScorePanel(scr, snake.getLength(), g_highScore);
    // 体力区：标签 STA + 当前/上限（位于分数数字下方，与分数同为“实时状态”）
    renderer.renderStaminaPanel(scr, 96, Snake::MAX_STAMINA, snake.getStamina());
}

void SingleMode::render()
{
    // 擦除上一帧的食物像素：吃到食物后该格不再标记 FOOD，但屏幕上那块黄色像素
    // 不会被任何逻辑清除（增量渲染只擦蛇尾），会一直残留，造成“食物吃不到 / 还在
    // 原地”的错觉。先按 EMPTY 擦掉上一帧食物位置，再画当前食物。
    const int cell_step = GameSpec::getCellSize() + GameSpec::getGap();
    const int cell_size = GameSpec::getCellSize();
    if (lastFoodPos.x >= 0 && lastFoodPos.y >= 0 &&
        lastFoodPos.x < board.getWidth() && lastFoodPos.y < board.getHeight())
    {
        scr->draw_retangle(BOARD_X0 + cell_step * lastFoodPos.x,
                           BOARD_Y0 + cell_step * lastFoodPos.y,
                           cell_size, cell_size, Color[EMPTY]);
    }
    renderer.render_snake_food(board, snake, food.getPos(), scr);
    lastFoodPos = food.getPos();

    // 每帧刷新体力数值（体力随每步移动变化，标签 STA 已在 render_full 画好）
    renderer.renderStaminaOnly(scr, 108, Snake::MAX_STAMINA, snake.getStamina());
}

void SingleMode::initGame()
{
    // 根据当前 GameSpec 配置重建 Board，确保尺寸与配置同步
    int screenH = scr ? scr->getHeight() : GameSpec::SCREEN_H_DEFAULT;
    int screenW = scr ? scr->getWidth()  : GameSpec::BOARD_RENDER_W;
    board = Board(GameSpec::getGridW(screenW), GameSpec::getGridH(screenH));

    // 重新建立墙壁
    for (int i = 0; i < board.getWidth(); i++)
    {
        board.setCellType({i, 0}, CellType::WALL);
        board.setCellType({i, board.getHeight() - 1}, CellType::WALL);
    }
    for (int i = 0; i < board.getHeight(); i++)
    {
        board.setCellType({0, i}, CellType::WALL);
        board.setCellType({board.getWidth() - 1, i}, CellType::WALL);
    }

    // 有效草地格子数 = 内部活动区 = (宽-2)*(高-2)，纯计算、只算一次
    freeCells = (board.getWidth() - 2) * (board.getHeight() - 2);
    won = false;

    // 蛇头必须落在墙内活动区 [2, width-3]×[2, height-3]。
    // 防止开头撞死在墙上，
    // 原 rand_int(3, width-3) 因 min>max 触发交换，把蛇头放到墙上/越界，
    // 导致“画面没渲染/没食物”的意外。这里把区间夹到合法活动区，退化时落到唯一合法格。
    int headMinX = 2, headMaxX = board.getWidth() - 3;
    int headMinY = 2, headMaxY = board.getHeight() - 3;
    if (headMinX > headMaxX) headMaxX = headMinX;
    if (headMinY > headMaxY) headMaxY = headMinY;
    Position head = { rand_int(headMinX, headMaxX), rand_int(headMinY, headMaxY) };

    // 蛇尾同样要落在墙内活动区，避免大格子下尾巴压到墙（原 offset 法在窄网格会越界）。
    Position offsets[4] = { {0, -1}, {0, 1}, {-1, 0}, {1, 0} };
    int idx = rand_int(0, 3);            // 决定初始朝向
    Position tail = head + offsets[idx];
    // 窄网格下 offsets[idx] 可能压到墙/越界，回退到第一个合法活动区方向，
    // 并同步 idx 使初始朝向与尾巴方向一致。
    if (!(tail.x >= 1 && tail.x <= board.getWidth() - 2 &&
          tail.y >= 1 && tail.y <= board.getHeight() - 2))
    {
        for (int k = 0; k < 4; k++)
        {
            Position t = head + offsets[k];
            if (t.x >= 1 && t.x <= board.getWidth() - 2 &&
                t.y >= 1 && t.y <= board.getHeight() - 2)
            {
                tail = t;
                idx = k;
                break;
            }
        }
    }
    snake = Snake(head, tail);

    Direction initDir;
    switch (idx)
    {
    case 0: initDir = Direction::DOWN;  break;
    case 1: initDir = Direction::UP;    break;
    case 2: initDir = Direction::RIGHT; break;
    case 3: initDir = Direction::LEFT;  break;
    }
    snake.setDirection(initDir);
    // 用配置中的起步速度初始化
    snake.set_speed(GameSpec::getStartSpeed());

    // 重置食物状态：否则上一局遗留的 exist==true 会让 spawnFood 直接 return，
    // 导致切格子尺寸后食物坐标仍是旧棋盘的坐标（越界），表现为“改尺寸后食物生成不了”。
    food.setExist(false);
    spawnFood();
    for (auto it = snake.getBody().begin(); it != snake.getBody().end(); ++it)
    {
        board.setCellType(*it, CellType::SNAKE_BODY);
    }
    board.setCellType(snake.get_new_head(), CellType::SNAKE_HEAD);
    board.setCellType(food.getPos(), CellType::FOOD);

    // 注意：initGame 在构造/重置时也会被调用，这里【不】直接渲染，
    // 由引擎在切换状态后调用 SingleMode::render() 绘制首帧，
    // 避免引擎构造阶段先画出对局、再被菜单覆盖（导致首屏闪现贪吃蛇）。

    lastFoodPos = food.getPos();   // 初始化，避免首帧增量渲染误擦墙格
}

void SingleMode::spawnFood()
{
    if (food.getExist())
        return;

    int max = 1000;
    while (max--)
    {
        food.update(board.getWidth(), board.getHeight());
        if (board.isValidPosition(food.getPos()) && board.getCellType(food.getPos()) == CellType::EMPTY)
        {
            board.setCellType(food.getPos(), CellType::FOOD);
            food.setExist(true);
            break;
        }
    }
    if (!food.getExist())
    {
        for (int x = 1; x < board.getWidth() - 1; x++)
        {
            for (int y = 1; y < board.getHeight() - 1; y++)
            {
                if (board.getCellType({x, y}) == CellType::EMPTY)
                {
                    // 同步 food.pos 与棋盘 FOOD 标记：否则渲染用 food.getPos()（仍是随机值）
                    // 而棋盘标记在 {x,y}，两者错位 → 蛇走到食物格也判定吃不到，表现为不长。
                    food.setPos(Position{ x, y });
                    board.setCellType(food.getPos(), CellType::FOOD);
                    food.setExist(true);
                    break;
                }
            }
            if (food.getExist()) break;
        }
    }

    // 无论走随机分支还是回退分支，只要食物成功生成，就刷新 20 秒计时起点
    if (food.getExist())
        foodSpawnTime = std::chrono::steady_clock::now();
}

bool SingleMode::checkCollision()
{
    Position head = snake.get_new_head();

    if (head.x < 1 || head.x > board.getWidth() - 2 ||
        head.y < 1 || head.y > board.getHeight() - 2)
    {
        return true;
    }

    auto body = snake.getBody();
    for (size_t i = 1; i < body.size(); ++i)
    {
        if (head == body[i])
            return true;
    }
    return false;
}

// 存档：保存当前完整游戏状态
void SingleMode::saveState()
{
    saveData.snakeBody = snake.getBody();
    saveData.snakeDirection = snake.getDirection();
    saveData.snakeGrowing = snake.isGrowing();
    saveData.snakeDead = snake.is_dead();
    saveData.snakeNewHead = snake.get_new_head();
    saveData.snakeOldTail = snake.getOldTail();
    saveData.snakeSpeed = snake.getSpeed();
    saveData.snakeStamina = snake.getStamina();

    saveData.foodPos = food.getPos();
    saveData.foodExist = food.getExist();

    saveData.boardCells.clear();
    for (int y = 0; y < board.getHeight(); ++y)
    {
        for (int x = 0; x < board.getWidth(); ++x)
        {
            Position pos{ x, y };
            CellType type = board.getCellType(pos);
            if (type != CellType::EMPTY)
            {
                saveData.boardCells.push_back({ pos, type });
            }
        }
    }

    saveData.win = won;
    saveData.valid = true;
    hasSave = true;
}

// 恢复：加载存档状态
void SingleMode::loadState()
{
    if (!saveData.valid)
        return;

    // 1) 恢复蛇的完整状态
    snake.restore(saveData.snakeBody,
                  saveData.snakeDirection,
                  saveData.snakeGrowing,
                  saveData.snakeDead,
                  saveData.snakeNewHead,
                  saveData.snakeOldTail,
                  saveData.snakeSpeed,
                  saveData.snakeStamina);

    // 2) 恢复食物（重建 Food 以复用其位置/尺寸/颜色封装）
    food = Food(saveData.foodPos, GameSpec::getCellSize(), Color[FOOD]);
    food.setExist(saveData.foodExist);
    // 恢复后重置食物 20 秒计时起点，避免沿用旧局的时间点导致恢复即刷新
    foodSpawnTime = std::chrono::steady_clock::now();

    // 3) 恢复棋盘网格占用信息
    for (int y = 0; y < board.getHeight(); ++y)
    {
        for (int x = 0; x < board.getWidth(); ++x)
        {
            board.setCellType({x, y}, CellType::EMPTY);
        }
    }
    for (const auto& rec : saveData.boardCells)
    {
        board.setCellType(rec.pos, rec.type);
    }

    // 4) 未结束对局：恢复后继续运行
    hasSave = true;
    gameOver = false;

    // 恢复获胜状态，并重算有效草地格数（由棋盘尺寸决定，纯计算）
    won = saveData.win;
    freeCells = (board.getWidth() - 2) * (board.getHeight() - 2);
}

// 根据当前 GameSpec 配置重新初始化游戏（格子大小变更时调用）
void SingleMode::recalcBoard()
{
    initGame();
}

// 清除存档（不重置对局逻辑）：修改配置后调用，使“继续游戏”自然变灰不可继续
void SingleMode::clearSave()
{
    hasSave = false;
    saveData.valid = false;
}

// 重置为新游戏
void SingleMode::resetGame()
{
    gameOver = false;
    hasSave = false;
    saveData.valid = false;
    initGame();
}

int SingleMode::getHighScore()
{
    return g_highScore;
}

void SingleMode::updateHighScore(int score)
{
    if (score > g_highScore)
        g_highScore = score;
}

void SingleMode::boostSpeed()
{
    snake.toggleSpeed();
}