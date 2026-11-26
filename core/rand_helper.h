#ifndef RAND_HELPER_H
#define RAND_HELPER_H
#include <random>

// 声明全局随机引擎，所有文件共用
extern std::mt19937 g_rng;

// [min,max]整数
int rand_int(int min, int max);

#endif