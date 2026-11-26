

#include "board.h"


Board::Board(int w, int h) : width(w), height(h)
{
    grid.resize(height, std::vector<CellType>(width, CellType::EMPTY));

    for (int i = 0; i < width; i++)
    {
        grid[0][i] = CellType::WALL;
        grid[height - 1][i] = CellType::WALL;
    }
    for (int i = 0; i < height; i++)
    {
        grid[i][0] = CellType::WALL;
        grid[i][width - 1] = CellType::WALL;
    }
}