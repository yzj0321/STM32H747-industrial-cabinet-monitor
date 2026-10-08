/**
 * @file    lcd_ltdc.c
 * @brief   LTDC RGB LCD 底层驱动（DMA2D 加速）
 */

#include "lcd_ltdc.h"
#include "lcd.h"
#include "bsp_delay.h"


uint32_t *g_ltdc_framebuf[2];              /* LTDC LCD帧缓存数组指针,必须指向对应大小的内存区域 */
_ltdc_dev lcdltdc;                         /* 管理LCD LTDC的重要参数 */


/**
 * @brief       LTDC开关
 * @param       sw   : 1,打开; 0,关闭;
 * @retval      无
 */
void ltdc_switch(uint8_t sw)
{
    if (sw)
    {
        __HAL_LTDC_ENABLE(&hltdc);   /* 打开LTDC */
    }
    else
    {
        __HAL_LTDC_DISABLE(&hltdc);  /* 关闭LTDC */
    }
}

/**
 * @brief       LTDC开关指定层
 * @param       layerx      : 0,第一层; 1,第二层;
 * @param       sw          : 1,打开;   0,关闭;
 * @retval      无
 */
void ltdc_layer_switch(uint8_t layerx, uint8_t sw)
{
    if (sw) 
    {
        __HAL_LTDC_LAYER_ENABLE(&hltdc, layerx);   /* 开启layerx */
    }
    else
    {
        __HAL_LTDC_LAYER_DISABLE(&hltdc, layerx);  /* 关闭layerx */
    }

    __HAL_LTDC_RELOAD_CONFIG(&hltdc);              /* 立即重新加载配置 */
}

/**
 * @brief       LTDC选择层
 * @param       layerx   : 层号:0,第一层; 1,第二层;
 * @retval      无
 */
void ltdc_select_layer(uint8_t layerx)
{
    lcdltdc.activelayer = layerx;
}

/**
 * @brief       LTDC显示方向设置
 * @param       dir         : 0,竖屏; 1,横屏;
 * @retval      无
 */
void ltdc_display_dir(uint8_t dir)
{
    lcdltdc.dir = dir;     /* 显示方向 */

    if (dir == 0)          /* 竖屏 */
    {
        lcdltdc.width = lcdltdc.pheight;
        lcdltdc.height = lcdltdc.pwidth;
    }
    else if (dir == 1)     /* 横屏 */
    {
        lcdltdc.width = lcdltdc.pwidth;
        lcdltdc.height = lcdltdc.pheight;
    }
}

/**
 * @brief       LTDC画点函数
 * @param       x,y         : 写入坐标
 * @param       color       : 颜色值
 * @retval      无
 */
void ltdc_draw_point(uint16_t x, uint16_t y, uint32_t color)
{ 
#if LTDC_PIXFORMAT == LTDC_PIXFORMAT_ARGB8888

    if (lcdltdc.dir)    /* 横屏 */
    {
        *(uint32_t *)((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * y + x)) = color;
    }
    else                /* 竖屏 */
    {
        *(uint32_t *)((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * (lcdltdc.pheight - x - 1) + y)) = color;
    }

#elif LTDC_PIXFORMAT == LTDC_PIXFORMAT_RGB888

    if (lcdltdc.dir)    /* 横屏 */
    {
        *(uint16_t *)((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * y + x)) = color;
        *(uint8_t *)((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * y + x) + 2) = color >> 16;
    }
    else                /* 竖屏 */
    {
        *(uint16_t *)((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * (lcdltdc.pheight - x - 1) + y)) = color;
        *(uint8_t *)((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * (lcdltdc.pheight - x - 1) + y) + 2) = color >> 16;
    }
    
#else

    if (lcdltdc.dir)    /* 横屏 */
    {
        *(uint16_t *)((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * y + x)) = color;
    }
    else                /* 竖屏 */
    {
        *(uint16_t *)((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * (lcdltdc.pheight - x - 1) + y)) = color;
    }

#endif
}

/**
 * @brief       LTDC读点函数
 * @param       x,y       : 读取点的坐标
 * @retval      颜色值
 */
uint32_t ltdc_read_point(uint16_t x, uint16_t y)
{ 
#if LTDC_PIXFORMAT == LTDC_PIXFORMAT_ARGB8888

    if (lcdltdc.dir)    /* 横屏 */
    {
        return *(uint32_t *)((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * y + x));
    }
    else                /* 竖屏 */
    {
        return *(uint32_t *)((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * (lcdltdc.pheight - x - 1) + y));
    }

#elif LTDC_PIXFORMAT == LTDC_PIXFORMAT_RGB888

    if (lcdltdc.dir)    /* 横屏 */
    {
        return *(uint32_t *)((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * y + x)) & 0XFFFFFF;
    }
    else                /* 竖屏 */
    {
        return *(uint32_t *)((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * (lcdltdc.pheight - x - 1) + y)) & 0XFFFFFF;
    }
    
#else

    if (lcdltdc.dir)    /* 横屏 */
    {
        return *(uint16_t *)((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * y + x));
    }
    else                /* 竖屏 */
    {
        return *(uint16_t *)((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * (lcdltdc.pheight - x - 1) + y));
    }

#endif
}

/**
 * @brief       LTDC填充矩形, DMA2D填充
 * @note       (sx,sy),(ex,ey):填充矩形对角坐标,区域大小为:(ex - sx + 1) * (ey - sy + 1)
 *              注意:sx,ex,不能大于lcddev.width - 1; sy,ey,不能大于lcddev.height - 1
 * @param       sx,sy       : 起始坐标
 * @param       ex,ey       : 结束坐标
 * @param       color       : 填充的颜色
 * @retval      无
 */
void ltdc_fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint32_t color)
{ 
    uint32_t psx, psy, pex, pey;        /* 以LCD面板为基准的坐标系,不随横竖屏变化而变化 */
    uint32_t timeout = 0; 
    uint16_t offline;
    uint32_t addr; 

    /* 坐标系转换 */
    if (lcdltdc.dir)                    /* 横屏 */
    {
        psx = sx;
        psy = sy;
        pex = ex;
        pey = ey;
    }
    else                                /* 竖屏 */
    {
        if (ex >= lcdltdc.pheight)
        {
            ex = lcdltdc.pheight - 1;   /* 限制范围 */
        }

        if (sx >= lcdltdc.pheight)
        {
            sx = lcdltdc.pheight - 1;   /* 限制范围 */
        }
        
        psx = sy;
        psy = lcdltdc.pheight - ex - 1;
        pex = ey;
        pey = lcdltdc.pheight - sx - 1;
    }

    offline = lcdltdc.pwidth - (pex - psx + 1);   /* 行偏移:当前行最后一个像素和下一行第一个像素之间的像素数目 */
    addr = ((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * psy + psx));

    __HAL_RCC_DMA2D_CLK_ENABLE();                             /* 使能DMA2D时钟 */

    DMA2D->CR &= ~(DMA2D_CR_START);                           /* 先停止DMA2D */
    DMA2D->CR = DMA2D_R2M;                                    /* 寄存器到存储器模式 */
    DMA2D->OPFCCR = LTDC_PIXFORMAT;                           /* 设置颜色格式 */
    DMA2D->OOR = offline;                                     /* 设置行偏移  */

    DMA2D->OMAR = addr;                                       /* 输出存储器地址 */
    DMA2D->NLR = (pey - psy + 1) | ((pex - psx + 1) << 16);   /* 设定行数寄存器 */
    DMA2D->OCOLR = color;                                     /* 设定输出颜色寄存器 */
    DMA2D->CR |= DMA2D_CR_START;                              /* 启动DMA2D */

    while ((DMA2D->ISR & (DMA2D_FLAG_TC)) == 0)               /* 等待传输完成 */
    {
        timeout++;
      
        if (timeout > 0X1FFFFF)break;                         /* 超时退出 */
    } 
    
    DMA2D->IFCR |= DMA2D_FLAG_TC;                             /* 清除传输完成标志 */
}

///* 使用DMA2D相关的HAL库函数使用DMA2D外设(不推荐) */
///**
// * @brief       在指定区域内填充单个颜色
// * @param       (sx,sy),(ex,ey)  : 填充矩形对角坐标,区域大小为:(ex - sx + 1) * (ey - sy + 1)
// * @param       color            : 要填充的颜色
// * @retval      无
// */
//void ltdc_fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint32_t color)
//{
//    uint32_t psx, psy, pex, pey;   /* 以LCD面板为基准的坐标系,不随横竖屏变化而变化 */
//    uint32_t timeout = 0; 
//    uint16_t offline;
//    uint32_t addr;  
//
//    if (ex >= lcdltdc.width)
//    {
//        ex = lcdltdc.width - 1;
//    }
//
//    if (ey >= lcdltdc.height)
//    {
//        ey = lcdltdc.height - 1;
//    }
//
//    /* 坐标系转换 */
//    if (lcdltdc.dir)   /* 横屏 */
//    {
//        psx = sx; 
//        psy = sy;
//        pex = ex;
//        pey = ey;
//    }
//    else               /* 竖屏 */
//    {
//        psx = sy;
//        psy = lcdltdc.pheight - ex - 1;
//        pex = ey;
//        pey = lcdltdc.pheight - sx - 1;
//    }

//    offline = lcdltdc.pwidth - (pex - psx + 1);
//    addr =((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * psy + psx));
//
//    if (LTDC_PIXFORMAT == LTDC_PIXEL_FORMAT_RGB565)  /* 如果是RGB565格式的话需要进行颜色转换，将16bit转换为32bit的 */
//    {
//        color = ((color & 0X0000F800) << 8) | ((color & 0X000007E0) << 5 ) | ((color & 0X0000001F) << 3);
//    }
//
//    /* 配置DMA2D的模式 */
//    g_dma2d_handle.Instance = DMA2D;
//    g_dma2d_handle.Init.Mode = DMA2D_R2M;                                                    /* 寄存器到存储器模式 */
//    g_dma2d_handle.Init.ColorMode = LTDC_PIXFORMAT;                                          /* 设置颜色格式 */
//    g_dma2d_handle.Init.OutputOffset = offline;                                              /* 输出偏移  */
//    HAL_DMA2D_Init(&g_dma2d_handle);                                                         /* 初始化DMA2D */
//
//    HAL_DMA2D_ConfigLayer(&g_dma2d_handle, lcdltdc.activelayer);                             /* 层配置 */
//    HAL_DMA2D_Start(&g_dma2d_handle, color, (uint32_t)addr, pex - psx + 1, pey - psy + 1);   /* 开启传输 */
//    HAL_DMA2D_PollForTransfer(&g_dma2d_handle, 1000);                                        /* 传输数据 */
//
//    while((__HAL_DMA2D_GET_FLAG( &g_dma2d_handle,DMA2D_FLAG_TC) == 0) && (timeout < 0X5000)) /* 等待DMA2D传输完成 */
//    {
//        timeout++;
//    }
//    __HAL_DMA2D_CLEAR_FLAG(&g_dma2d_handle,DMA2D_FLAG_TC);                                   /* 清除传输完成标志 */
//}

/**
 * @brief       在指定区域内填充指定颜色块, DMA2D填充
 * @note        此函数仅支持uint16_t,RGB565格式的颜色数组填充.
 *              (sx,sy),(ex,ey):填充矩形对角坐标,区域大小为:(ex - sx + 1) * (ey - sy + 1)
 *              注意:sx,ex,不能大于lcddev.width - 1; sy,ey,不能大于lcddev.height - 1
 * @param       sx,sy       : 起始坐标
 * @param       ex,ey       : 结束坐标
 * @param       color       : 填充的颜色数组首地址
 * @retval      无
 */
void ltdc_color_fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t *color)
{
    uint32_t psx, psy, pex, pey;   /* 以LCD面板为基准的坐标系,不随横竖屏变化而变化 */
    uint32_t timeout = 0; 
    uint16_t offline;
    uint32_t addr;
  
    /* 坐标系转换 */
    if (lcdltdc.dir)               /* 横屏 */
    {
        psx = sx;
        psy = sy;
        pex = ex;
        pey = ey;
    }
    else                           /* 竖屏 */
    {
        psx = sy;
        psy = lcdltdc.pheight - ex - 1;
        pex = ey;
        pey = lcdltdc.pheight - sx - 1;
    }

    offline = lcdltdc.pwidth - (pex - psx + 1);   /* 行偏移:当前行最后一个像素和下一行第一个像素之间的像素数目 */
    addr = ((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * psy + psx));

    __HAL_RCC_DMA2D_CLK_ENABLE();                             /* 使能DMA2D时钟 */

    DMA2D->CR &= ~(DMA2D_CR_START);                           /* 先停止DMA2D */
    DMA2D->CR = DMA2D_M2M;                                    /* 存储器到存储器模式 */
    DMA2D->FGPFCCR = LTDC_PIXFORMAT;                          /* 设置前景层颜色格式 */
    DMA2D->FGOR = 0;                                          /* 前景层行偏移为0 */
    DMA2D->OOR = offline;                                     /* 设置行偏移 */

    DMA2D->FGMAR = (uint32_t)color;                           /* 源地址 */
    DMA2D->OMAR = addr;                                       /* 输出存储器地址 */
    DMA2D->NLR = (pey - psy + 1) | ((pex - psx + 1) << 16);   /* 设定行数寄存器 */
    DMA2D->CR |= DMA2D_CR_START;                              /* 启动DMA2D */

    while((DMA2D->ISR & (DMA2D_FLAG_TC)) == 0)                /* 等待传输完成 */
    {
        timeout++;
      
        if (timeout > 0X1FFFFF)break;                         /* 超时退出 */
    } 
    
    DMA2D->IFCR |= DMA2D_FLAG_TC;                             /* 清除传输完成标志 */
}  

/**
 * @brief       LTDC清屏
 * @param       color       : 颜色值
 * @retval      无
 */
void ltdc_clear(uint32_t color)
{
    ltdc_fill(0, 0, lcdltdc.width - 1, lcdltdc.height - 1, color);
}

/**
 * @brief       LTDC时钟(Fdclk)设置函数
 * @param       pll3n     : PLL3 VCO倍频系数(PLL倍频),        取值范围:4~512.
 * @param       pll3m     : PLL3预分频系数(进PLL之前的分频),  取值范围:1~63.
 * @param       pll3r     : PLL3的r分频系数(PLL之后的分频),   取值范围:1~128.
 *
 * @note        Fvco = Fs * (pll3n / pll3m);
 *              Fr = Fvco / pll3r = Fs * (pll3n / (pll3m * pll3r));
 *              Fdclk = Fr;
 *              其中:
 *              Fvco: VCO频率
 *              Fr: PLL3的r分频输出时钟频率
 *              Fs: PLL3输入时钟频率,可以是HSI,CSI,HSE等(系统时钟初始化时选择HSE作为PLL的输入时钟源).
 *
 *              假设:外部晶振为25M, pllm = 25 的时候, Fs = 25Mhz， pllm分频后频率 为1Mhz.
 *              例如: 要得到33M的LTDC时钟, 则可以设置: pll3n = 300, pllm = 25, pll3r = 9
 *              Fdclk= ((25 / 25) * 300) / 9 = 33 Mhz
 * @retval      0, 成功;
 *              1, 失败;
 */
uint8_t ltdc_clk_set(uint32_t pll3n, uint32_t pll3m, uint32_t pll3r)
{
    RCC_PeriphCLKInitTypeDef periphclk_initure;

    /* LTDC输出像素时钟，需要根据自己所使用的LCD数据手册来配置！ */
    periphclk_initure.PeriphClockSelection = RCC_PERIPHCLK_LTDC;     /* LTDC时钟 */
    periphclk_initure.PLL3.PLL3M = pll3m;
    periphclk_initure.PLL3.PLL3N = pll3n;
    periphclk_initure.PLL3.PLL3P = 2;
    periphclk_initure.PLL3.PLL3Q = 2;
    periphclk_initure.PLL3.PLL3R = pll3r;

    if (HAL_RCCEx_PeriphCLKConfig(&periphclk_initure) == HAL_OK)     /* 配置像素时钟 */
    {
        return 0;                                                    /* 成功 */
    }
    else 
    {
        return 1;                                                    /* 失败 */
    }
}

/**
 * @brief       LTDC层窗口设置, 窗口以LCD面板坐标系为基准
 * @note        此函数必须在ltdc_layer_parameter_config之后再设置.另外,当设置的窗口值不等于面板的尺
 *              寸时,GRAM的操作(读/写点函数),也要根据窗口的宽高来进行修改,否则显示不正常(本例程就未做修改).
 * @param       layerx      : 0,第一层; 1,第二层;
 * @param       sx, sy      : 起始坐标
 * @param       width,height: 宽度和高度
 * @retval      无
 */
void ltdc_layer_window_config(uint8_t layerx, uint16_t sx, uint16_t sy, uint16_t width, uint16_t height)
{
    HAL_LTDC_SetWindowPosition(&hltdc, sx, sy, layerx);     /* 设置窗口的位置 */
    HAL_LTDC_SetWindowSize(&hltdc, width, height, layerx);  /* 设置窗口大小 */
  
    if (lcdltdc.pheight == 1280 && layerx == 0)
    {
        LTDC_Layer1->CFBLR = (width * 4 << 16) | (width * 4 + 7);   /* 帧缓冲区行长和行间距设置(以字节为单位) */
    }  
}

/**
 * @brief       LTDC层基本参数设置
 * @note        此函数,必须在ltdc_layer_window_config之前设置.
 * @param       layerx      : 0,第一层; 1,第二层;
 * @param       bufaddr     : 层颜色帧缓存起始地址
 * @param       pixformat   : 层像素格式. 0,ARGB8888; 1,RGB888; 2,RGB565; 3,ARGB1555; 4,ARGB4444; 5,L8; 6;AL44; 7;AL88
 * @param       alpha       : 层恒定Alpha值, 0,全透明;255,不透明
 * @param       alpha0      : 默认颜色Alpha值, 0,全透明;255,不透明
 * @param       bfac1       : 混合系数1, 4(100),恒定Alpha; 6(101),像素Alpha*恒定Alpha
 * @param       bfac2       : 混合系数2, 5(101),恒定Alpha; 7(111),像素Alpha*恒定Alpha
 * @param       bkcolor     : 层默认颜色,32位,低24位有效,RGB888格式
 * @retval      无
 */
void ltdc_layer_parameter_config(uint8_t layerx, uint32_t bufaddr, uint8_t pixformat, uint8_t alpha, uint8_t alpha0, uint8_t bfac1, uint8_t bfac2, uint32_t bkcolor)
{
    LTDC_LayerCfgTypeDef playercfg;

    playercfg.WindowX0 = 0;                                            /* 窗口起始X坐标 */
    playercfg.WindowY0 = 0;                                            /* 窗口起始Y坐标 */
    playercfg.WindowX1 = lcdltdc.pwidth;                               /* 窗口终止X坐标 */
    playercfg.WindowY1 = lcdltdc.pheight;                              /* 窗口终止Y坐标 */
    playercfg.PixelFormat = pixformat;                                 /* 设置层像素格式 */
    playercfg.Alpha = alpha;                                           /* 设置层恒定Alpha值,0~255,硬件255分频,255为完全不透明 */
    playercfg.Alpha0 = alpha0;                                         /* 设置默认颜色Alpha值 */
    playercfg.BlendingFactor1 = (uint32_t)bfac1 << 8;                  /* 设置层混合系数1 */
    playercfg.BlendingFactor2 = (uint32_t)bfac2;                       /* 设置层混合系数2 */
    playercfg.FBStartAdress = bufaddr;                                 /* 设置层颜色帧缓存起始地址 */
    playercfg.ImageWidth = lcdltdc.pwidth;                             /* 设置颜色帧缓冲区的宽度 */
    playercfg.ImageHeight = lcdltdc.pheight;                           /* 设置颜色帧缓冲区的高度 */
    playercfg.Backcolor.Red = (uint8_t)(bkcolor & 0X00FF0000) >> 16;   /* 背景颜色红色部分 */
    playercfg.Backcolor.Green = (uint8_t)(bkcolor & 0X0000FF00) >> 8;  /* 背景颜色绿色部分 */
    playercfg.Backcolor.Blue = (uint8_t)bkcolor & 0X000000FF;          /* 背景颜色蓝色部分 */
    HAL_LTDC_ConfigLayer(&hltdc, &playercfg, layerx);          /* 设置所选中的层 */
}  

/**
 * @brief       LTDC读取面板ID
 * @note        利用LCD RGB线的最高位(R7,G7,B7)来识别面板ID
 *              PJ6 = R7(M0); PK2 = G7(M1); PK6 = B7(M2);
 *              M2:M1:M0
 *              0 :0 :0     4.3 寸480*272  RGB屏,ID = 0X4342
 *              0 :0 :1     7   寸800*480  RGB屏,ID = 0X7084
 *              0 :1 :0     7   寸1024*600 RGB屏,ID = 0X7016
 *              0 :1 :1     5.5 寸720*1280 RGB屏,ID = 0X5571
 *              1 :0 :0     4.3 寸800*480  RGB屏,ID = 0X4384
 *              1 :0 :1     10.1寸1280*800 RGB屏,ID = 0X1018
 * @param       无
 * @retval      0, 非法; 
 *              其他, LCD ID
 */
uint16_t ltdc_panelid_read(void)
{
    uint8_t idx = 0;

    GPIO_InitTypeDef gpio_init_struct;

    __HAL_RCC_GPIOJ_CLK_ENABLE();                              /* 使能GPIOJ时钟 */
    __HAL_RCC_GPIOK_CLK_ENABLE();                              /* 使能GPIOK时钟 */
    
    gpio_init_struct.Pin = GPIO_PIN_6;                         /* R7引脚PJ6 */
    gpio_init_struct.Mode = GPIO_MODE_INPUT;                   /* 输入 */
    gpio_init_struct.Pull = GPIO_PULLUP;                       /* 上拉 */
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;        /* 高速 */
    HAL_GPIO_Init(GPIOJ, &gpio_init_struct);                   /* 初始化PJ6 */
    
    gpio_init_struct.Pin = GPIO_PIN_2 | GPIO_PIN_6;            /* G7,B7引脚PK2,6 */
    HAL_GPIO_Init(GPIOK, &gpio_init_struct);                   /* 初始化PK2,6 */

    delay_us(10);
    idx  = (uint8_t)HAL_GPIO_ReadPin(GPIOJ, GPIO_PIN_6);       /* 读取M0 */
    idx |= (uint8_t)HAL_GPIO_ReadPin(GPIOK, GPIO_PIN_2) << 1;  /* 读取M1 */
    idx |= (uint8_t)HAL_GPIO_ReadPin(GPIOK, GPIO_PIN_6) << 2;  /* 读取M2 */

    switch (idx)
    {
        case 0 :
            return 0X4342;                    /* 4.3寸屏,480*272分辨率 */
        
        case 1 :
            return 0X7084;                    /* 7  寸屏,800*480分辨率 */
        
        case 2 :
            return 0X7016;                    /* 7  寸屏,1024*600分辨率 */
        
        case 3 :
            return 0X5571;                    /* 5.5寸屏,720*1280分辨率 */
        
        case 4 :
            return 0X4384;                    /* 4.3寸屏,800*480分辨率 */
        
        case 5 :
            return 0X1018;                    /* 10.1寸屏,1280*800分辨率 */
        
        default :
            return 0;
    }
}

/**
 * @brief       LTDC初始化函数
 * @param       无
 * @retval      无
 */
void ltdc_init(void)
{
    uint16_t lcdid = 0;

    ltdc_gpio_init();
    lcdid = ltdc_panelid_read();    /* 读取LCD面板ID */

#if RGB_80_8001280         
    lcdid = 0X8081;
#endif
  
    if (lcdid == 0X4342)
    {
        lcdltdc.pwidth = 480;       /* 面板宽度,单位:像素 */
        lcdltdc.pheight = 272;      /* 面板高度,单位:像素 */
        lcdltdc.hsw = 1;            /* 水平同步宽度 */
        lcdltdc.hbp = 40;           /* 水平后廊 */
        lcdltdc.hfp = 5;            /* 水平前廊 */      
        lcdltdc.vsw = 1;            /* 垂直同步宽度 */
        lcdltdc.vbp = 8;            /* 垂直后廊 */      
        lcdltdc.vfp = 8;            /* 垂直前廊 */
        ltdc_clk_set(300, 25, 33);  /* 设置像素时钟  9Mhz */
    }
    else if (lcdid == 0X7084)
    {
        lcdltdc.pwidth = 800;       /* 面板宽度,单位:像素 */
        lcdltdc.pheight = 480;      /* 面板高度,单位:像素 */
        lcdltdc.hsw = 1;            /* 水平同步宽度 */
        lcdltdc.hbp = 46;           /* 水平后廊 */
        lcdltdc.hfp = 210;          /* 水平前廊 */
        lcdltdc.vsw = 1;            /* 垂直同步宽度 */
        lcdltdc.vbp = 23;           /* 垂直后廊 */
        lcdltdc.vfp = 22;           /* 垂直前廊 */
        ltdc_clk_set(300, 25, 9);   /* 设置像素时钟 33Mhz(如果开双显,需要降低DCLK到18.75Mhz,才会比较好) */
    }
    else if (lcdid == 0X7016)
    {
        lcdltdc.pwidth = 1024;      /* 面板宽度,单位:像素 */
        lcdltdc.pheight = 600;      /* 面板高度,单位:像素 */
        lcdltdc.hsw = 20;           /* 水平同步宽度 */
        lcdltdc.hbp = 140;          /* 水平后廊 */
        lcdltdc.hfp = 160;          /* 水平前廊 */
        lcdltdc.vsw = 3;            /* 垂直同步宽度 */
        lcdltdc.vbp = 20;           /* 垂直后廊 */
        lcdltdc.vfp = 12;           /* 垂直前廊 */
        ltdc_clk_set(300, 25, 6);   /* 设置像素时钟  50Mhz */
    }
    else if (lcdid == 0X5571)
    {
        lcdltdc.pwidth = 720;       /* 面板宽度,单位:像素 */
        lcdltdc.pheight = 1280;     /* 面板高度,单位:像素 */
        lcdltdc.hsw = 10;           /* 水平同步宽度 */
        lcdltdc.hbp = 36;           /* 水平后廊 */
        lcdltdc.hfp = 46;           /* 水平前廊 */
        lcdltdc.vsw = 5;            /* 垂直同步宽度 */
        lcdltdc.vbp = 5;            /* 垂直后廊 */
        lcdltdc.vfp = 16;           /* 垂直前廊 */
        ltdc_clk_set(330, 25, 6);   /* 设置像素时钟  55Mhz */
    }
    else if (lcdid == 0X4384)
    {
        lcdltdc.pwidth = 800;       /* 面板宽度,单位:像素 */
        lcdltdc.pheight = 480;      /* 面板高度,单位:像素 */
        lcdltdc.hsw = 48;           /* 水平同步宽度 */      
        lcdltdc.hbp = 88;           /* 水平后廊 */
        lcdltdc.hfp = 40;           /* 水平前廊 */
        lcdltdc.vsw = 3;            /* 垂直同步宽度 */
        lcdltdc.vbp = 32;           /* 垂直后廊 */
        lcdltdc.vfp = 13;           /* 垂直前廊 */
        ltdc_clk_set(300, 25, 9);   /* 设置像素时钟 33Mhz */ 
    }
    else if (lcdid == 0X8081)       /* 8寸800*1280 RGB屏 */
    {
        lcdltdc.pwidth = 800;       /* 面板宽度,单位:像素 */
        lcdltdc.pheight = 1280;     /* 面板高度,单位:像素 */
        lcdltdc.hsw = 5;            /* 水平同步宽度 */
        lcdltdc.hbp = 20;           /* 水平后廊 */
        lcdltdc.hfp = 40;           /* 水平前廊 */
        lcdltdc.vsw = 3;            /* 垂直同步宽度 */
        lcdltdc.vbp = 20;           /* 垂直后廊 */
        lcdltdc.vfp = 30;           /* 垂直前廊 */
        ltdc_clk_set(300, 25, 5);   /* 设置像素时钟  60Mhz */
    }
    else if (lcdid == 0X1018)       /* 10.1寸1280*800 RGB屏 */
    {
        lcdltdc.pwidth = 1280;      /* 面板宽度,单位:像素 */
        lcdltdc.pheight = 800;      /* 面板高度,单位:像素 */
        lcdltdc.hsw = 10;           /* 水平同步宽度 */
        lcdltdc.hbp = 140;          /* 水平后廊 */
        lcdltdc.hfp = 10;           /* 水平前廊 */
        lcdltdc.vsw = 3;            /* 垂直同步宽度 */
        lcdltdc.vbp = 10;           /* 垂直后廊 */
        lcdltdc.vfp = 10;           /* 垂直前廊 */
        ltdc_clk_set(300, 25, 5);   /* 设置像素时钟  60Mhz */
    } 


    lcddev.width = lcdltdc.pwidth;      /* 设置lcddev的宽度参数 */
    lcddev.height = lcdltdc.pheight;    /* 设置lcddev的高度参数 */
    lcdltdc.pixformat = LTDC_PIXFORMAT; /* 颜色像素格式 */

    g_ltdc_framebuf[0] = (uint32_t *)LTDC_FRAME_BUF_ADDR;
    lcdltdc.pixsize = 2;

#if !BSP_USE_CUBEMX_LTDC
    hltdc.Instance = LTDC;

    if (lcdid == 0X8081)
    {
        hltdc.Init.HSPolarity = LTDC_HSPOLARITY_AH;     /* 水平同步极性:高电平有效 */
    }
    else
    {
        hltdc.Init.HSPolarity = LTDC_HSPOLARITY_AL;     /* 水平同步极性:低电平有效 */
    }

    hltdc.Init.VSPolarity = LTDC_VSPOLARITY_AL;         /* 垂直同步极性:低电平有效 */
    hltdc.Init.DEPolarity = LTDC_DEPOLARITY_AL;         /* 数据使能极性:低电平有效 */
    hltdc.State = HAL_LTDC_STATE_RESET;

    if (lcdid == 0X1018 || lcdid == 0X8081)
    {
        hltdc.Init.PCPolarity = LTDC_PCPOLARITY_IIPC;   /* 像素时钟极性:高电平有效 */
    }
    else
    {
        hltdc.Init.PCPolarity = LTDC_PCPOLARITY_IPC;    /* 像素时钟极性:低电平有效 */
    }

    hltdc.Init.HorizontalSync = lcdltdc.hsw - 1;
    hltdc.Init.VerticalSync = lcdltdc.vsw - 1;
    hltdc.Init.AccumulatedHBP = lcdltdc.hsw + lcdltdc.hbp - 1;
    hltdc.Init.AccumulatedVBP = lcdltdc.vsw + lcdltdc.vbp - 1;
    hltdc.Init.AccumulatedActiveW = lcdltdc.hsw + lcdltdc.hbp + lcdltdc.pwidth - 1;
    hltdc.Init.AccumulatedActiveH = lcdltdc.vsw + lcdltdc.vbp + lcdltdc.pheight - 1;
    hltdc.Init.TotalWidth = lcdltdc.hsw + lcdltdc.hbp + lcdltdc.pwidth + lcdltdc.hfp - 1;
    hltdc.Init.TotalHeigh = lcdltdc.vsw + lcdltdc.vbp + lcdltdc.pheight + lcdltdc.vfp - 1;
    hltdc.Init.Backcolor.Red = 0;
    hltdc.Init.Backcolor.Green = 0;
    hltdc.Init.Backcolor.Blue = 0;
    HAL_LTDC_Init(&hltdc);
#endif

    ltdc_layer_parameter_config(0, (uint32_t)g_ltdc_framebuf[0], LTDC_PIXFORMAT, 255, 0, 6, 7, 0X000000);
    ltdc_layer_window_config(0, 0, 0, lcdltdc.pwidth, lcdltdc.pheight);

    ltdc_display_dir(1);
    ltdc_select_layer(0);
    LTDC_RST(1);
    delay_ms(10);
    LTDC_RST(0);
    delay_ms(50);
    LTDC_RST(1);
    delay_ms(200);

    LTDC_BL(1);
    ltdc_clear(0XFFFFFFFF);
}


/**
 * @brief       LTDC底层GPIO初始化(背光/复位/供电)
 * @retval      无
 */
void ltdc_gpio_init(void)
{
    GPIO_InitTypeDef gpio_init_struct = {0};

    LTDC_BL_GPIO_CLK_ENABLE();
    LTDC_RST_GPIO_CLK_ENABLE();
    LCD_PWREN_GPIO_CLK_ENABLE();

    gpio_init_struct.Pin = LTDC_BL_GPIO_PIN;
    gpio_init_struct.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init_struct.Pull = GPIO_PULLUP;
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(LTDC_BL_GPIO_PORT, &gpio_init_struct);

    gpio_init_struct.Pin = LTDC_RST_GPIO_PIN;
    HAL_GPIO_Init(LTDC_RST_GPIO_PORT, &gpio_init_struct);

    gpio_init_struct.Pin = LCD_PWREN_GPIO_PIN;
    HAL_GPIO_Init(LCD_PWREN_GPIO_PORT, &gpio_init_struct);

    LCD_PWREN(1);
}
