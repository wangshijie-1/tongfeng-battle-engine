
#include "World_Unit.h"

// 格子像素大小：19+1=20步(最小), 29+1=30步(中), 49+1=50步(最大)
// gridW = BOARD_RENDER_W / cellStep，动态计算，无需存储
const int GameSpec::cellSizes[3] = { 19, 29, 49 };

// 起步速度档位
const int GameSpec::startSpeedOptions[3] = { 20, 25, 35 };
// 按 R 后的加速档位
const int GameSpec::boostSpeedOptions[3] = { 35, 50, 70 };

// 当前配置索引（默认为每项的第一档）
int GameSpec::cellSizeIndex       = 0;
int GameSpec::startSpeedIndex     = 0;
int GameSpec::boostSpeedIndex     = 0;
