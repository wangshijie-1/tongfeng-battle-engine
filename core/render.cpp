#include "render.h"
#include "Font.h"

void ScreenRender::render(const Board& board, const Snake& Snake,
    const Position& food, Screen* scr)
{
    scr->clear(0x00);
    int x0, y0;
    int cell_color;
    const int cell_size = GameSpec::getCellSize();
    const int cell_gap  = GameSpec::getGap();
    const int cell_step = cell_size + cell_gap;

    for (int y = 0; y < board.getHeight(); y++)
    {
        for (int x = 0; x < board.getWidth(); x++)
        {
            Position pos{ x, y };
            CellType cell_type = board.getCellType(pos);
            x0 = BOARD_X0 + cell_step * x;
            y0 = BOARD_Y0 + cell_step * y;

            // CellType 枚举值与 Color[] 下标一一对应，直接按下标取色（数据驱动）：
            // 后续新增格子类型只需在 CellType 与 Color 各加一项，无需改动此处渲染逻辑。
            const int ci = static_cast<int>(cell_type);
            cell_color = (ci >= 0 && ci < CellType::WU) ? Color[ci] : 0x00;

            scr->draw_retangle(x0, y0, cell_size, cell_size, cell_color);
        }
    }
}


void ScreenRender::render_snake_food(const Board& board, const Snake& snake,
    const Position& food, Screen* scr)
{
    const int cell_step = GameSpec::getCellSize() + GameSpec::getGap();
    const int cell_size = GameSpec::getCellSize();

    // 擦除蛇尾
    if (!snake.isGrowing())
    {
        scr->draw_retangle(BOARD_X0 + cell_step * snake.getOldTail().x,
            BOARD_Y0 + cell_step * snake.getOldTail().y,
            cell_size, cell_size, Color[EMPTY]);
    }

    // 画食物
    scr->draw_retangle(BOARD_X0 + cell_step * food.x,
        BOARD_Y0 + cell_step * food.y,
        cell_size, cell_size, Color[FOOD]);

    // 画蛇头
    const Position& head = snake.get_new_head();
    scr->draw_retangle(BOARD_X0 + cell_step * head.x,
        BOARD_Y0 + cell_step * head.y,
        cell_size, cell_size, Color[SNAKE_HEAD]);

    // 画蛇头后一节
    if (snake.getBody().size() > 1)
    {
        const Position& next_head = snake.getBody()[1];
        scr->draw_retangle(BOARD_X0 + cell_step * next_head.x,
            BOARD_Y0 + cell_step * next_head.y,
            cell_size, cell_size, Color[SNAKE_BODY]);
    }
}

// ============================================================================
// 右侧分数侧栏渲染
//   位于棋盘右侧 124px 空白区（BOARD_X0 + SCREEN_W = 900 起）
//   renderScorePanel：初始化侧栏（黑色背景 + 描边 + 标签 + 数字），仅调用一次
//   renderScoreOnly：只更新数字像素，不重绘背景/边框/标签
// ============================================================================
void ScreenRender::renderScorePanel(Screen* scr, int currentScore, int highScore)
{
    if (!scr) return;

    const int panelW = GameSpec::PANEL_W;
    const int panelX     = scr->getWidth() - panelW;
    const int panelH     = scr->getHeight();
    const int midY       = panelH / 2;
    const int scale      = 2;
    const int pad        = 16;

    // 1. 侧栏黑色背景（整块覆盖）
    scr->draw_retangle(panelX, 0, panelW, panelH, 0x000000);

    // 2. 青色描边矩形（四边各 4px 粗线）
    scr->draw_retangle(panelX, 0,                  panelW, 4,  0x00D4AA);
    scr->draw_retangle(panelX, panelH - 4,         panelW, 4,  0x00D4AA);
    scr->draw_retangle(panelX, 0,                  4,    panelH, 0x00D4AA);
    scr->draw_retangle(panelX + panelW - 4, 0,     4,    panelH, 0x00D4AA);

    // --- 上半区：SCORE 标签 + 分数 ---
    const char* scoreLabel = "SCORE";
    int labelW = Font::textWidth(scoreLabel, scale);
    Font::drawText(scr, panelX + (panelW - labelW) / 2, pad,
                   scoreLabel, 0x00D4AA, scale);

    char scoreStr[16];
    snprintf(scoreStr, sizeof(scoreStr), "%d", currentScore);
    int scoreW = Font::textWidth(scoreStr, scale);
    Font::drawText(scr, panelX + (panelW - scoreW) / 2,
                   pad + 16 * scale + 8, scoreStr, 0xFFFFFF, scale);

    // --- 下半区：HIS 标签 + 最高分 ---
    const char* hisLabel = "HIS";
    int hisLabelW = Font::textWidth(hisLabel, scale);
    Font::drawText(scr, panelX + (panelW - hisLabelW) / 2,
                   midY + pad, hisLabel, 0x00D4AA, scale);

    snprintf(scoreStr, sizeof(scoreStr), "%d", highScore);
    int hisW = Font::textWidth(scoreStr, scale);
    Font::drawText(scr, panelX + (panelW - hisW) / 2,
                   midY + pad + 16 * scale + 8,
                   scoreStr, 0xFFFFFF, scale);
}

// 仅更新分数和最高分的数字区域，不碰背景和边框
// 用于每次吃食物后刷新数字，避免整块重绘覆盖游戏画面
void ScreenRender::renderScoreOnly(Screen* scr, int currentScore, int highScore)
{
    if (!scr) return;

    const int panelW = GameSpec::PANEL_W;
    const int panelX     = scr->getWidth() - panelW;
    const int panelH     = scr->getHeight();
    const int midY       = panelH / 2;
    const int scale      = 2;
    const int pad        = 16;

    // 将数字区域先清黑（清除旧数字残留）
    const int numY_top    = pad + 16 * scale + 8;   // 当前分数文字起始行
    const int numH        = 16 * scale;              // 数字区域高度（1个汉字高度）
    const int numY_bottom = midY + pad + 16 * scale + 8; // 最高分文字起始行

    // 清当前分数区域（预留足够宽度防止旧数字残留）
    scr->draw_retangle(panelX + 4, numY_top, panelW - 8, numH, 0x000000);
    // 清最高分区域
    scr->draw_retangle(panelX + 4, numY_bottom, panelW - 8, numH, 0x000000);

    char scoreStr[16];
    snprintf(scoreStr, sizeof(scoreStr), "%d", currentScore);
    int scoreW = Font::textWidth(scoreStr, scale);
    Font::drawText(scr, panelX + (panelW - scoreW) / 2, numY_top,
                   scoreStr, 0xFFFFFF, scale);

    snprintf(scoreStr, sizeof(scoreStr), "%d", highScore);
    int hisW = Font::textWidth(scoreStr, scale);
    Font::drawText(scr, panelX + (panelW - hisW) / 2, numY_bottom,
                   scoreStr, 0xFFFFFF, scale);
}

// 按蛇自身颜色，整条绘制一条蛇（头用 headColor，身用 bodyColor）
// 用于双人模式首帧让两条蛇以不同颜色区分
void ScreenRender::renderSnake(const Board& board, const Snake& snake, Screen* scr)
{
    const int cell_step = GameSpec::getCellSize() + GameSpec::getGap();
    const int cell_size = GameSpec::getCellSize();
    const auto& body = snake.getBody();

    for (size_t i = 0; i < body.size(); i++)
    {
        const Position& p = body[i];
        int color = (i == 0) ? snake.getHeadColor() : snake.getBodyColor();
        scr->draw_retangle(BOARD_X0 + cell_step * p.x,
                           BOARD_Y0 + cell_step * p.y,
                           cell_size, cell_size, color);
    }
}

// ============================================================================
// 右侧侧栏体力区渲染
//   标签 "STA"（青色，scale=1）+ 数值 "当前/上限"（按剩余比例着色，居中）
//   renderStaminaPanel：标签 + 数值（侧栏初始化时调用一次）
//   renderStaminaOnly：只刷新数值区域（每帧/体力变化时调用，不碰背景边框）
//   颜色语义：>=60% 绿(充足)、>=30% 黄(偏低)、<30% 红(告急)
// ============================================================================
static int staminaTextColor(int cur, int max)
{
    if (max <= 0) return 0xFFFFFF;
    int pct = cur * 100 / max;
    if (pct >= 60) return 0x00FF00;   // 充足：绿
    if (pct >= 30) return 0xFFFF00;   // 偏低：黄
    return 0xFF3030;                  // 告急：红
}

void ScreenRender::renderStaminaPanel(Screen* scr, int y, int maxStamina, int currentStamina)
{
    if (!scr) return;
    const int panelW = GameSpec::PANEL_W;
    const int panelX = scr->getWidth() - panelW;

    // 标签 STA（与侧栏描边同色的青色）
    const char* label = "STA";
    int lw = Font::textWidth(label, 1);
    Font::drawText(scr, panelX + (panelW - lw) / 2, y, label, 0x00D4AA, 1);

    // 数值画在标签下方 12px
    renderStaminaOnly(scr, y + 12, maxStamina, currentStamina);
}

void ScreenRender::renderStaminaOnly(Screen* scr, int y, int maxStamina, int currentStamina)
{
    if (!scr) return;
    const int panelW = GameSpec::PANEL_W;
    const int panelX = scr->getWidth() - panelW;

    // 清数值区（防旧数字残留）
    scr->draw_retangle(panelX + 4, y, panelW - 8, 16, 0x000000);

    char buf[16];
    snprintf(buf, sizeof(buf), "%d/%d", currentStamina, maxStamina);
    int tw = Font::textWidth(buf, 1);
    Font::drawText(scr, panelX + (panelW - tw) / 2, y, buf,
                   staminaTextColor(currentStamina, maxStamina), 1);
}