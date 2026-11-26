


#include "food.h"

Food::Food(Position pos,int size,int color) :
    pos(pos),
    exist(false),
    size(size),
    color(color)
{
    
}


Food::~Food()
{
    
}

void Food::setExist(bool exist0)
{
    exist = exist0;
}

bool Food::getExist()
{
    return exist;
}

