
#include <iostream>
#include <cstring>
#include <cerrno>



#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>
#include <linux/fb.h>
#include <sys/ioctl.h>

#include <mutex>
#include "Screen.h"

using namespace std;


Screen* Screen::scr = nullptr; //
std::mutex scr_m; //专门用来保护 Screen::scr的

Screen::Screen(const char* _dev_file):
    file(_dev_file), fd(-1), width(0), height(0), plcd(nullptr)
{
    int ret;
    //(1) 打开设备 open
    fd = open(file, O_RDWR);
    if (fd == -1)
    {
     
       std::cerr << "failed to open " 
                << file << " " << 
                std::strerror(errno) << std::endl;


        return;
    }

  

    //(2) 获取屏幕信息，如： 分辨率
    struct fb_var_screeninfo var;
    ret = ioctl(fd, FBIOGET_VSCREENINFO, &var);
    if (ret == -1)
    {
        perror("failed to FBIOGET_VSCREENINFO");

        std::cerr << "failed to FBIOGET_VSCREENINFO "
                  << std::strerror(errno) << std::endl;
        return;
    }
    // var.xres  var.yres  这个屏幕的分辨率
    // var.bits_per_pixel 每个像素点所占的bit位数
    // 整个帧缓冲区的大小: var.xres * var.yres * (var.bits_per_pixel/8)
    width = var.xres;
    height = var.yres;
    bytes_per_pixel = var.bits_per_pixel / 8;
    // printf("resultion: %d x %d\n", lcd_width, lcd_height);
    // printf("bytes_per_pixel: %d\n", bytes_per_pixel);

    // (3) mmap
    plcd = (int *)mmap(
        nullptr,                                     // 映射地址，NULL表示让系统自行分配
        width * height * bytes_per_pixel, // 要映射的长度
        PROT_WRITE,                               // 映射区域的权限
        MAP_SHARED,                               // 映射方式
        fd,                                       // 文件描述符，表示您要映射哪个文件
        0                                         // 偏移量，表示要从文件的哪个位置开始映射
    );
    if (plcd == MAP_FAILED)
    {
        perror("mmap error");
        std::cerr << "mmap error " 
            << std::strerror(errno) << std::endl;
        return;
    }
}

Screen::~Screen()
{
    munmap(plcd, width * height * bytes_per_pixel);
    close(fd);



    scr = nullptr;
}