#ifndef FONT_H
#define FONT_H

#include "Screen.h"

/**
 * @brief 极简位图字体库（满足菜单/暂停/结算等少量文字显示需求）。
 *
 * 设计要点：
 *  - 英文/数字/符号：8x8 点阵（ASCII，32~90 部分字符）。
 *  - 中文：16x16 点阵（仅内置少量常用汉字，可按需扩展）。
 *  - 字符串按 UTF-8 解析：单字节走英文点阵，三字节（0xE0 引导）走中文点阵。
 *  - 所有绘制基于 Screen::draw_point，因此渲染尺寸自然跟随屏幕物理分辨率，
 *    不硬编码宽高（符合渲染规范）。
 *
 * 说明：中文字模为最小手绘起始集，仅用于打通文字渲染链路；
 *       生产环境如需完整/正确字形，建议改用 FreeType + .ttf 字体替换本模块。
 */
class Font
{
public:
    // 在 (x, y) 以 color 颜色、scale 缩放绘制 UTF-8 字符串（自动换行关闭，单行）
    static void drawText(Screen* scr, int x, int y, const char* text, int color, int scale = 1);

    // 计算字符串像素宽度（用于居中布局），scale 越大越宽
    static int textWidth(const char* text, int scale = 1);

    // 单个英文/数字字符宽度
    static int asciiWidth(int scale = 1) { return 8 * scale; }

    // 单个汉字宽度
    static int cnWidth(int scale = 1) { return 16 * scale; }

private:
    static void drawAscii(Screen* scr, int x, int y, char c, int color, int scale);
    static void drawCn(Screen* scr, int x, int y, unsigned int codepoint, int color, int scale);
    static const unsigned short* findCn(unsigned int codepoint);  // 找不到返回 nullptr
};

#endif
