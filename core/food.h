#ifndef FOOD_H
#define FOOD_H

#include "World_Unit.h"
#include "rand_helper.h"

class Food
{
public:
    
    Food(Position pos,int size = 5,int color = 0xFFFF00);
    ~Food();

    inline Position update(int w, int h)        // 更新食物位置（避开墙壁）
    {
        // 收敛边界：保证 min<=max 且坐标落在 [1, w-2]/[1, h-2] 的界内 EMPTY 区。
        // 否则窄屏大格子下 w-2<1 会让 rand_int 触发 min>max 交换，落点越界被 setCellType
        // 静默丢弃，导致“食物压根不生成、蛇吃不到不长”。
        int mx = (w - 2 > 1) ? (w - 2) : 1;
        int my = (h - 2 > 1) ? (h - 2) : 1;
        pos.x = rand_int(1, mx);
        pos.y = rand_int(1, my);
        return pos;
    }

    Position getPos() const { return pos; }     // 获取食物位置
    void setPos(const Position& p) { pos = p; } // 设置食物位置（供落子兜底同步用）

    void setExist(bool exist0);                 // 设置食物是否存在
    bool getExist();                            // 获取食物是否存在
private:
    
    Position pos;       // 食物位置
    int color;          // 食物颜色
    int size;           // 食物大小
    int exist;          // 食物是否存在
    int time;           // 食物存在时间
};






#endif 