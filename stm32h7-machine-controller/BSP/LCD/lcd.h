/**
 * @file    lcd.h
 * @brief   LCD 显示应用层接口与颜色定义
 */

#ifndef __LCD_H
#define __LCD_H

#include "stdlib.h"
#include "main.h"
#include "lcd_ltdc.h"


/******************************************************************************************/
/* LCD PWREN 引脚 定义 
 * LCD_PWREN引脚用于IO控制LCD_5V的开启关闭, 开启LCD_5V需要将LCD_PWREN引脚输出1.
 */

#define LCD_PWREN_GPIO_PORT               GPIOI
#define LCD_PWREN_GPIO_PIN                GPIO_PIN_11
#define LCD_PWREN_GPIO_CLK_ENABLE()       do{ __HAL_RCC_GPIOI_CLK_ENABLE(); }while(0)   /* PI口时钟使能 */

/* LCD电源控制引脚 */
#define LCD_PWREN(x)      do{ x ? \
                              HAL_GPIO_WritePin(LCD_PWREN_GPIO_PORT, LCD_PWREN_GPIO_PIN, GPIO_PIN_SET) : \
                              HAL_GPIO_WritePin(LCD_PWREN_GPIO_PORT, LCD_PWREN_GPIO_PIN, GPIO_PIN_RESET); \
                          }while(0)

/******************************************************************************************/

/* LCD重要参数集 */
typedef struct
{
    uint16_t width;     /* LCD 宽度 */
    uint16_t height;    /* LCD 高度 */
    uint16_t id;        /* LCD ID */
    uint8_t dir;        /* 横屏还是竖屏控制：0，竖屏；1，横屏。 */
} _lcd_dev;

/* LCD参数 */
extern _lcd_dev lcddev; /* 管理LCD重要参数 */

/* LCD的画笔颜色和背景色 */
extern uint32_t  g_point_color;     /* 默认红色 */
extern uint32_t  g_back_color;      /* 背景颜色.默认为白色 */

/* LCD背光控制 */
#define LCD_BL(x)      do{ x ? \
                           HAL_GPIO_WritePin(LTDC_BL_GPIO_PORT, LTDC_BL_GPIO_PIN, GPIO_PIN_SET) : \
                           HAL_GPIO_WritePin(LTDC_BL_GPIO_PORT, LTDC_BL_GPIO_PIN, GPIO_PIN_RESET); \
                       }while(0)

/* LCD复位引脚 */
#define LCD_RST(x)     do{ x ? \
                           HAL_GPIO_WritePin(LTDC_RST_GPIO_PORT, LTDC_RST_GPIO_PIN, GPIO_PIN_SET) : \
                           HAL_GPIO_WritePin(LTDC_RST_GPIO_PORT, LTDC_RST_GPIO_PIN, GPIO_PIN_RESET); \
                       }while(0)

/******************************************************************************************/
/* LCD颜色 定义 */

#if LTDC_PIXFORMAT == LTDC_PIXEL_FORMAT_RGB565

/* 常用画笔颜色 */
#define WHITE           0xFFFF          /* 白色 */
#define BLACK           0x0000          /* 黑色 */
#define RED             0xF800          /* 红色 */
#define GREEN           0x07E0          /* 绿色 */
#define BLUE            0x001F          /* 蓝色 */ 
#define MAGENTA         0XF81F          /* 品红色/紫红色 = BLUE + RED */
#define YELLOW          0XFFE0          /* 黄色 = GREEN + RED */
#define CYAN            0X07FF          /* 青色 = GREEN + BLUE */  

/* 非常用颜色 */
#define BROWN           0XBC40          /* 棕色 */
#define BRRED           0XFC07          /* 棕红色 */
#define GRAY            0X8430          /* 灰色 */ 
#define DARKBLUE        0X01CF          /* 深蓝色 */
#define LIGHTBLUE       0X7D7C          /* 浅蓝色 */ 
#define GRAYBLUE        0X5458          /* 灰蓝色 */ 
#define LIGHTGREEN      0X841F          /* 浅绿色 */  
#define LGRAY           0XC618          /* 浅灰色(PANNEL),窗体背景色 */ 
#define LGRAYBLUE       0XA651          /* 浅灰蓝色(中间层颜色) */ 
#define LBBLUE          0X2B12          /* 浅棕蓝色(选择条目的反色) */ 

#elif LTDC_PIXFORMAT == LTDC_PIXFORMAT_RGB888

/* 常用画笔颜色 */
#define WHITE           0xFFFFFF        /* 白色 */
#define BLACK           0x000000        /* 黑色 */
#define RED             0xFF0000        /* 红色 */
#define GREEN           0x00FF00        /* 绿色 */
#define BLUE            0x0000FF        /* 蓝色 */ 
#define MAGENTA         0XFF00FF        /* 品红色/紫红色 = BLUE + RED */
#define YELLOW          0XFFFF00        /* 黄色 = GREEN + RED */
#define CYAN            0X00FFFF        /* 青色 = GREEN + BLUE */  

/* 非常用颜色 */
#define BROWN           0xB88800        /* 棕色 */
#define BRRED           0XF88038        /* 棕红色 */
#define GRAY            0X808480        /* 灰色 */ 
#define DARKBLUE        0X003878        /* 深蓝色 */
#define LIGHTBLUE       0X78ACE0        /* 浅蓝色 */ 
#define GRAYBLUE        0X5088C0        /* 灰蓝色 */ 
#define LIGHTGREEN      0X8080F8        /* 浅绿色 */  
#define LGRAY           0XC0C0C0        /* 浅灰色(PANNEL),窗体背景色 */ 
#define LGRAYBLUE       0XA0C888        /* 浅灰蓝色(中间层颜色) */ 
#define LBBLUE          0x286090        /* 浅棕蓝色(选择条目的反色) */

#elif LTDC_PIXFORMAT == LTDC_PIXFORMAT_ARGB8888

#define WHITE           0xFFFFFFFF      /* 白色 */
#define BLACK           0xFF000000      /* 黑色 */
#define RED             0xFFFF0000      /* 红色 */
#define GREEN           0xFF00FF00      /* 绿色 */
#define BLUE            0xFF0000FF      /* 蓝色 */ 
#define MAGENTA         0XFFFF00FF      /* 品红色/紫红色 = BLUE + RED */
#define YELLOW          0XFFFFFF00      /* 黄色 = GREEN + RED */
#define CYAN            0XFF00FFFF      /* 青色 = GREEN + BLUE */  

#define BROWN           0xFFB88800      /* 棕色 */
#define BRRED           0XFFF88038      /* 棕红色 */
#define GRAY            0XFF808480      /* 灰色 */ 
#define DARKBLUE        0XFF003878      /* 深蓝色 */
#define LIGHTBLUE       0XFF78ACE0      /* 浅蓝色 */ 
#define GRAYBLUE        0XFF5088C0      /* 灰蓝色 */ 
#define LIGHTGREEN      0XFF8080F8      /* 浅绿色 */  
#define LGRAY           0XFFC0C0C0      /* 浅灰色(PANNEL),窗体背景色 */ 
#define LGRAYBLUE       0XFFA0C888      /* 浅灰蓝色(中间层颜色) */ 
#define LBBLUE          0xFF286090      /* 浅棕蓝色(选择条目的反色) */

#endif

/******************************************************************************************/
/* 函数声明 */

/** @brief 初始化 LCD（RGB/LTDC） */
void lcd_init(void);
/** @brief 开启 LCD 显示 */
void lcd_display_on(void);
/** @brief 关闭 LCD 显示 */
void lcd_display_off(void);
/** @brief 设置扫描方向（RGB 屏无效） @param dir 扫描方向 */
void lcd_scan_dir(uint8_t dir);
/** @brief 设置显示方向 @param dir 0 竖屏，1 横屏 */
void lcd_display_dir(uint8_t dir);

/** @brief 准备写 GRAM（RGB 屏为空实现） */
void lcd_write_ram_prepare(void);
/** @brief 设置光标（RGB 屏无效） @param x X 坐标 @param y Y 坐标 */
void lcd_set_cursor(uint16_t x, uint16_t y);
/** @brief RGB565 转 RGB888 @param rgb565 输入颜色 @retval RGB888 颜色值 */
uint32_t lcd_rgb565torgb888(uint16_t rgb565);
/** @brief 读取像素 @param x X 坐标 @param y Y 坐标 @retval 颜色值 */
uint32_t lcd_read_point(uint16_t x, uint16_t y);
/** @brief 画点 @param x X 坐标 @param y Y 坐标 @param color 颜色 */
void lcd_draw_point(uint16_t x, uint16_t y, uint32_t color);

/** @brief 清屏 @param color 填充颜色 */
void lcd_clear(uint32_t color);
/** @brief 填充实心圆 @param x 圆心 X @param y 圆心 Y @param r 半径 @param color 颜色 */
void lcd_fill_circle(uint16_t x, uint16_t y, uint16_t r, uint32_t color);
/** @brief 画圆 @param x0 圆心 X @param y0 圆心 Y @param r 半径 @param color 颜色 */
void lcd_draw_circle(uint16_t x0, uint16_t y0, uint8_t r, uint32_t color);
/** @brief 画水平线 @param x 起点 X @param y 起点 Y @param len 长度 @param color 颜色 */
void lcd_draw_hline(uint16_t x, uint16_t y, uint16_t len, uint32_t color);
/** @brief 设置窗口（RGB 屏无效） */
void lcd_set_window(uint16_t sx, uint16_t sy, uint16_t width, uint16_t height);
/** @brief 纯色填充矩形 */
void lcd_fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint32_t color);
/** @brief 彩色填充矩形 @param color RGB565 像素数组 */
void lcd_color_fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t *color);
/** @brief 画直线 */
void lcd_draw_line(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint32_t color);
/** @brief 画矩形边框 */
void lcd_draw_rectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint32_t color);

/** @brief 显示单个字符 */
void lcd_show_char(uint16_t x, uint16_t y, char chr, uint8_t size, uint8_t mode, uint32_t color);
/** @brief 显示数字（高位零不显示） */
void lcd_show_num(uint16_t x, uint16_t y, uint32_t num, uint8_t len, uint8_t size, uint32_t color);
/** @brief 扩展显示数字（可显示前导零） */
void lcd_show_xnum(uint16_t x, uint16_t y, uint32_t num, uint8_t len, uint8_t size, uint8_t mode, uint32_t color);
/** @brief 显示字符串 */
void lcd_show_string(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t size, char *p, uint32_t color);


#endif






