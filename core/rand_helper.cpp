

#include "rand_helper.h"
#include <chrono>

// 定义全局mt19937随机引擎，只实例化一次
std::mt19937 g_rng(std::chrono::steady_clock::now().time_since_epoch().count());

int rand_int(int min, int max)
{
    // 防御：当网格较小（例如大格子规格下 gridW<6）时，调用方可能传入 min>max，
    // 如 initGame 的 rand_int(3, board.getWidth()-3)。std::uniform_int_distribution
    // 在 a>b 时会抛 std::invalid_argument，导致 initGame 抛异常、对局无法初始化
    //（现象正是“没有食物 / 画面没渲染”）。这里统一交换，保证总有合法返回值，避免崩溃。
    if (min > max)
    {
        int t = min; min = max; max = t;
    }
    std::uniform_int_distribution<int> dist(min, max);
    return dist(g_rng);
}