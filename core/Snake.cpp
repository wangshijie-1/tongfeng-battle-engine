

#include "Snake.h"

using namespace std;


/**
 * @brief 蛇的默认构造函数
 */
Snake::Snake() :
    body{ {2,2},{2,3} },
    currentDirection(Direction::LEFT),
    nextDirection(Direction::LEFT),
    growing(false),
    dead(false),
    new_head({30,30}),
    old_tail({1,1}),
    speed(20),
    stamina(100)
{
    // 如果蛇头和蛇尾在同一行, 且蛇头在蛇尾的右边, 则默认向右
    if (body[0].y == body[1].y && body[0].x > body[1].x)
    {
        currentDirection = Direction::RIGHT;
        nextDirection = Direction::RIGHT;
    }
}

/**
 * @brief 蛇的构造函数,默认向左
 * 
 * @param head 蛇头位置
 * @param tail 蛇尾位置
*/
Snake::Snake(Position head, Position tail) :
    body{ head, tail },
    currentDirection(Direction::LEFT),
    nextDirection(Direction::LEFT),
    growing(false),
    dead(false),
    new_head(head),
    old_tail({ 1,1 }),
    speed(20),
    stamina(100)
{

}

/**
 * @brief 蛇的析构函数
 * 
*/
Snake::~Snake()
{

}

/**
 * @brief 蛇的移动函数
 * 
*/
void Snake::move()
{
    // 如果下一个方向和当前方向相反, 则不改变方向
    if (!isOppositeDirection(currentDirection, nextDirection))
    {
        currentDirection = nextDirection;
    }

    //先计算 new Head Position
    Position newHead = body.front();
    switch (currentDirection)
    {
    case Direction::UP: newHead.y--; break;
    case Direction::DOWN: newHead.y++;break;
    case Direction::LEFT: newHead.x--; break;
    case Direction::RIGHT: newHead.x++; break;

    }


    //Add new head
    body.push_front(newHead);
    new_head = newHead;

    //Remove tail(unless growing)
    if (!growing)
    {
        old_tail = body.back();
        body.pop_back();
    }
    else
    {
        growing = false;
    }

    // 体力逻辑（移动成功后判定）：加速状态每走一格消耗 1 点体力，
    // 正常状态每走一格恢复 1 点体力，体力值域限制在 [0, MAX_STAMINA]。
    // 放在 move() 内即可同时作用于单人 / 双人两种模式。
    if (isBoosting())
    {
        stamina = (stamina > 0) ? (stamina - 1) : 0;
        // 体力耗尽（到达 0 这一临界点）：强制解除加速、恢复正常速度，并禁止再次加速。
        // 消耗过程中不做额外判断，只有归零这一明确的临界才触发。
        if (stamina == 0)
        {
            canBoost = false;
            speed = GameSpec::getStartSpeed();
        }
    }
    else
    {
        stamina = (stamina < MAX_STAMINA) ? (stamina + 1) : MAX_STAMINA;
        // 恢复体力：一旦超过 10 点即恢复加速能力（符合直觉：体力充裕才允许加速）。
        // 只在恢复分支判断，不在消耗分支重复处理，减少代码干扰。
        if (stamina > 10)
            canBoost = true;
    }
}

/**
 * @brief 在指定位置/朝向/长度初始化蛇身（双人模式对角出生）。
 *        身体沿当前朝向的反方向延伸，蛇头在 head。
 *
 *        设计：Snake 是纯逻辑层（不持有 Board），蛇身→棋盘占用由所属 Mode 负责。
 *        因此本函数只初始化 body / 方向 / 速度 / 体力，不触碰 Board，
 *        保持与单人 Snake::move() 一致的“无 board 依赖”分层。
 */
void Snake::initAt(const Position& head, Direction dir, int len)
{
    body.clear();
    int dx = 0, dy = 0;
    switch (dir)
    {
    case Direction::UP:    dy = 1;  break;   // 身体在头下方
    case Direction::DOWN:  dy = -1; break;
    case Direction::LEFT:  dx = 1;  break;   // 身体在头右侧
    case Direction::RIGHT: dx = -1; break;
    default: break;
    }
    for (int i = 0; i < len; i++)
        body.push_back({ head.x + dx * i, head.y + dy * i });

    currentDirection = dir;
    nextDirection = dir;
    growing = false;
    dead = false;
    speed = GameSpec::getStartSpeed();
    stamina = 100;
    canBoost = true;   // 出生体力满值，允许加速

    // 出生即把 new_head / old_tail 置为真实值，保证 get_new_head() / getOldTail()
    // 在首帧移动前也有效（成熟范式依赖这两个值做增量渲染）。头=首节，尾=末节。
    new_head = body.front();
    old_tail = body.back();
}

/**
 *@brief 更新下一个方向，移动时在move()中拦截180°掉头
 * 
 * @param dir 新方向
 */
void Snake::changeDirection(Direction dir)
{
    nextDirection = dir;
}

/**
 *@brief 直接设置当前与下一个方向。用于开局/重置，绕过180°拦截
 * 
 * @param dir 新方向
 */
void Snake::setDirection(Direction dir)
{
    currentDirection = dir;
    nextDirection = dir;
}

/**
 *@brief 从存档恢复蛇的完整状态。
 *       “继续游戏”时由 SingleMode::loadState 调用，直接覆盖所有成员，
 *       避免逐格重建身体，保证恢复后的对局与退出前完全一致。
 *
 * @param body     蛇身（头在前，尾在后）
 * @param dir      当前/下一个方向
 * @param growing  是否在生长中
 * @param dead     是否死亡
 * @param newHead  新头部位置
 * @param oldTail  旧尾部位置
 * @param length   长度
 * @param speed    速度
 * @param stamina  体力
 */
void Snake::restore(const std::deque<Position>& body,
                    Direction dir,
                    bool growing,
                    bool dead,
                    const Position& newHead,
                    const Position& oldTail,
                    int speed,
                    int stamina)
{
    this->body = body;
    this->currentDirection = dir;
    this->nextDirection = dir;
    this->growing = growing;
    this->dead = dead;
    this->new_head = newHead;
    this->old_tail = oldTail;
    this->speed = speed;
    this->stamina = stamina;

    // 存档中无 canBoost 字段（兼容旧存档），按“体力 > 10 才允许加速”规则推断：
    // 体力不足时若存档残留加速档，强制恢复正常速度，保证规则一致。
    canBoost = (stamina > 10);
    if (!canBoost && this->speed == GameSpec::getBoostSpeed())
        this->speed = GameSpec::getStartSpeed();
}

// static const int 成员类外定义（在类内已有初始化值时，此定义满足 ODR 使用）
const int Snake::MAX_STAMINA;



