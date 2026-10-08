/**
 * @file lv_port_disp_templ.c
 *
 */

/* 复制此文件为 "lv_port_disp.c" 并将此值设为 "1" 以启用内容 */
#if 1

/*********************
 *      包含文件
 *********************/
#include "lvgl.h"  
#include "lv_port_disp_template.h"
#include "./BSP/LCD/lcd.h"
#include <stdint.h>
#include <stdbool.h>

/*********************
 *      宏定义
 *********************/
#define USE_SRAM        0       /* 使用外部SRAM为1，否则为0 */
#ifdef USE_SRAM
#include "./MALLOC/malloc.h"
#endif

#define MY_DISP_HOR_RES    800   /* 屏幕宽度 */
#define MY_DISP_VER_RES    480   /* 屏幕高度 */

/* 定义每像素字节数（RGB565为2字节） */
#define BYTE_PER_PIXEL 2
		
/**********************
 *      类型定义
 **********************/

/**********************
 *  静态函数声明
 **********************/
static void disp_init(void);

static void disp_flush(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map);

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *  GLOBAL FUNCTIONS
 **********************/

/**
 * @brief       初始化并注册显示设备
 * @param       无
 * @retval      无
 */
void lv_port_disp_init(void)
{
    /*-------------------------
     * 初始化显示设备
     * -----------------------*/
    disp_init();

    /*------------------------------------
     * 创建显示驱动并设置刷新回调函数
     * -----------------------------------*/
    //lv_display_t * disp = lv_display_create(MY_DISP_HOR_RES, MY_DISP_VER_RES); 
	  lv_display_t * disp = lv_display_create(lcddev.width, lcddev.height);
    lv_display_set_flush_cb(disp, disp_flush);

	  /* 示例1：单缓冲区 */
#if USE_SRAM                         /* 使用外部SRAM分配缓冲区 */  
    size_t buf_size = MY_DISP_HOR_RES * MY_DISP_VER_RES * sizeof(lv_color_t);
    lv_color_t *buf_1_1 = (lv_color_t *)mymalloc(SRAMEX, buf_size);
	  lv_display_set_buffers(disp, buf_1_1, NULL, buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);
#else
    static LV_ATTRIBUTE_MEM_ALIGN uint8_t buf_1_1[MY_DISP_HOR_RES * 400];         	
		lv_display_set_buffers(disp, buf_1_1, NULL, sizeof(buf_1_1), LV_DISPLAY_RENDER_MODE_PARTIAL);
#endif   

//    /* 示例2：
//     * 双缓冲区用于部分渲染
//     * 在flush_cb中应使用DMA或类似硬件在后台更新显示 */
//    LV_ATTRIBUTE_MEM_ALIGN
//    static uint8_t buf_2_1[MY_DISP_HOR_RES * 10 * BYTE_PER_PIXEL];
//
//    LV_ATTRIBUTE_MEM_ALIGN
//    static uint8_t buf_2_2[MY_DISP_HOR_RES * 10 * BYTE_PER_PIXEL];
//    lv_display_set_buffers(disp, buf_2_1, buf_2_2, sizeof(buf_2_1), LV_DISPLAY_RENDER_MODE_PARTIAL);
//
//    /* 示例3：
//     * 双缓冲全屏尺寸缓冲区
//     * LV_DISPLAY_RENDER_MODE_DIRECT和LV_DISPLAY_RENDER_MODE_FULL均可工作 */
//    LV_ATTRIBUTE_MEM_ALIGN
//    static uint8_t buf_3_1[MY_DISP_HOR_RES * MY_DISP_VER_RES * BYTE_PER_PIXEL];
//
//    LV_ATTRIBUTE_MEM_ALIGN
//    static uint8_t buf_3_2[MY_DISP_HOR_RES * MY_DISP_VER_RES * BYTE_PER_PIXEL];
//    lv_display_set_buffers(disp, buf_3_1, buf_3_2, sizeof(buf_3_1), LV_DISPLAY_RENDER_MODE_DIRECT);

}

/**********************
 *  静态函数
 **********************/

/**
 * @brief       初始化显示设备和必要的外围设备
 * @param       无
 * @retval      无
 */
static void disp_init(void)
{
    /* 在此添加你的初始化代码 */
	  lcd_init();         /* 初始化LCD */
    lcd_display_dir(1); /* 设置屏幕显示方向 */
}

volatile bool disp_flush_enabled = true;

/**
 * @brief       启用屏幕更新（当LVGL调用disp_flush()时执行刷新过程）
 * @param       无
 * @retval      无
 */
void disp_enable_update(void)
{
    disp_flush_enabled = true;
}

/**
 * @brief       禁用屏幕更新（当LVGL调用disp_flush()时不执行刷新过程）
 * @param       无
 * @retval      无
 */
void disp_disable_update(void)
{
    disp_flush_enabled = false;
}

/**
 * @brief    将内部缓冲区的内容刷新到显示器的指定区域
 *   @note   可使用DMA或硬件加速在后台执行，但完成后必须调用lv_display_flush_ready()
 * @param    disp_drv    : 显示设备
 *   @arg    area        : 要刷新的区域，包含了填充矩形的对角坐标
 *   @arg    px_map      : 包含渲染后的原始像素图，应复制到显示器的area区域
 *
 *  @retval  无
 */
static void disp_flush(lv_display_t * disp_drv, const lv_area_t * area, uint8_t * px_map)
{  
    if(lcddev.dir) 
		{  
        lcd_color_fill(area->x1, area->y1, area->x2, area->y2, (uint16_t*)px_map);
    } 
		else 
		{  
				#if LTDC_PIXFORMAT == LTDC_PIXFORMAT_ARGB8888
						uint32_t *buf = (uint32_t *)px_map;
				#else
						uint16_t *buf = (uint16_t *)px_map;
				#endif

				for(int y = area->y1; y <= area->y2; y++) 
				{
						for(int x = area->x1; x <= area->x2; x++) 
						{
								#if LTDC_PIXFORMAT == LTDC_PIXFORMAT_ARGB8888
														uint32_t color = *buf++;
								#elif LTDC_PIXFORMAT == LTDC_PIXFORMAT_RGB565
														uint16_t color = *buf++;
								#else
														uint16_t color = *buf++; 
								#endif
														ltdc_draw_point(x, y, color);
						}
				}
    }
    
    /* 必须调用刷新完成通知 */
    lv_display_flush_ready(disp_drv);
}

#else /* 在文件顶部启用此功能 */

/* 此虚拟类型定义仅用于消除-Wpedantic警告 */
typedef int keep_pedantic_happy;
#endif
