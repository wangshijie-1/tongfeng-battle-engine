#ifndef BOARD_H
#define BOARD_H

#include <vector>
#include "World_Unit.h"

class Board
{
private:
    int width;
    int height;

    // CellType grid[height][width];
    std::vector< std::vector<CellType> > grid;

public:
    Board(int w, int h);
    Board() : width(0), height(0) {}   // 默认构造（成员声明时/延迟初始化用，initGame 再赋值重建）

    inline bool isValidPosition(const Position& pos) const
    {
        return  pos.x >= 0 && pos.x < width &&
            pos.y >= 0 && pos.y < height;
    }

    CellType getCellType(const Position& pos) const
    {
        // 越界统一按墙处理：既避免未定义行为（原实现越界无 return），也与碰撞语义一致。
        if (!isValidPosition(pos))
            return CellType::WALL;
        return grid[pos.y][pos.x];
    }

    void setCellType(const Position& pos, CellType type)
    {
        if (isValidPosition(pos))
            grid[pos.y][pos.x] = type;
    }

    // 将所有格子重置为 EMPTY（不含墙壁重建，需手动再 set）
    void clear()
    {
        for (int y = 0; y < height; y++)
            for (int x = 0; x < width; x++)
                grid[y][x] = CellType::EMPTY;
    }

    int getHeight(void) const
    {
        // return grid.size();
        return height;
    }
    int getWidth(void) const
    {
        // return grid[0].size();
        return width;
    }

};










#endif 