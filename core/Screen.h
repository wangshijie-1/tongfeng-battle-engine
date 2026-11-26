
#ifndef __SCREEN_H__
#define __SCREEN_H__

#include <mutex>

extern std::mutex scr_m;
class Screen
{
private:
    const char* file; //屏幕的文件名
    int fd;  //文件描述符
    int width;  //屏幕分辨率 x轴
    int height; //屏幕分辨率 y轴
    int bytes_per_pixel; //每个像素占多少字节
    int *plcd;          //指针，指向帧缓冲区的首地址


    static Screen* scr;


private:
    Screen(const char* _dev_file = "/dev/fb0");

    Screen(const Screen& ) = delete;

    ~Screen();

public:

    //inline 画点
    inline void draw_point(int x, int y, int color)
    {
        *(plcd + y*width + x) = color;
    }

    //inline 画矩形（带越界裁剪：防止像素索引换行导致“食物卡在两格中间/错位”）
    inline void draw_retangle(int x, int y, int w, int h, int color)
    {
        for (int i = y; i < y + h; i++)
        {
            if (i < 0 || i >= height) continue;   // 越界行直接跳过，绝不换行写
            for (int j = x; j < x + w; j++)
            {
                if (j < 0 || j >= width) continue; // 越界列直接跳过
                draw_point(j, i, color);
            }
        }
    }


    //inline 清屏
    inline void clear(int color = 0x555555)
    {
        for (int y = 0; y < height; y++)
        {
            for (int x = 0; x < width; x++)
            {
                draw_point(x,y, color);
            }
        }
    }


    //获取屏幕对象
    static Screen* get_screen(void)
    {
        
        //记得要加锁
        std::unique_lock<std::mutex> l(scr_m);

        if (scr == nullptr)
        {
            scr = new Screen("/dev/fb0");
        }

        return scr;
    }

    // 对外公开接口：读取板子物理分辨率（渲染规范禁止硬编码，统一从此获取）
    inline int getWidth() const { return width; }
    inline int getHeight() const { return height; }

    // 对外公开接口：返回帧缓冲区首地址（暂停像素缓存还原用）
    inline int* getFrameBuffer() const { return plcd; }
};





#endif 