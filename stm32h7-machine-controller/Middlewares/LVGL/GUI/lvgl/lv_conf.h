/**
 * @file lv_conf.h
 * v9.3.0 �������ļ�
 */

/*
 * �����ļ�����Ϊ `lv_conf.h`
 * 1. ֱ�ӷ��� `lvgl` �ļ����Ա�
 * 2. ���Ƶ�����λ�ã�Ȼ��
 *    - ���� `LV_CONF_INCLUDE_SIMPLE`
 *    - ��·������Ϊ����·��
 */

/* clang-format off */
#if 1 /* ����Ϊ "1" ���������� */

#ifndef LV_CONF_H
#define LV_CONF_H

/* �����Ҫ�ڴ˴������κ����ݣ����� `__ASSEMBLY__` �����ڽ��� */
#if  0 && defined(__ASSEMBLY__)
#include "my_include.h"
#endif

/*====================
   ��ɫ����
 *====================*/

/** ��ɫ���: 1 (I1), 8 (L8), 16 (RGB565), 24 (RGB888), 32 (XRGB8888) */
#define LV_COLOR_DEPTH 16

/*=========================
   ��׼���װ����
 *=========================*/

/** ��ѡֵ
 * - LV_STDLIB_BUILTIN:     LVGL ����ʵ��
 * - LV_STDLIB_CLIB:        ��׼ C �������� malloc, strlen ��
 * - LV_STDLIB_MICROPYTHON: MicroPython ʵ��
 * - LV_STDLIB_RTTHREAD:    RT-Thread ʵ��
 * - LV_STDLIB_CUSTOM:      �ⲿʵ�ֺ��� */
#define LV_USE_STDLIB_MALLOC    LV_STDLIB_BUILTIN

/** ��ѡֵ
 * - LV_STDLIB_BUILTIN:     LVGL ����ʵ��
 * - LV_STDLIB_CLIB:        ��׼ C �������� malloc, strlen ��
 * - LV_STDLIB_MICROPYTHON: MicroPython ʵ��
 * - LV_STDLIB_RTTHREAD:    RT-Thread ʵ��
 * - LV_STDLIB_CUSTOM:      �ⲿʵ�ֺ��� */
#define LV_USE_STDLIB_STRING    LV_STDLIB_BUILTIN

/** ��ѡֵ
 * - LV_STDLIB_BUILTIN:     LVGL ����ʵ��
 * - LV_STDLIB_CLIB:        ��׼ C �������� malloc, strlen ��
 * - LV_STDLIB_MICROPYTHON: MicroPython ʵ��
 * - LV_STDLIB_RTTHREAD:    RT-Thread ʵ��
 * - LV_STDLIB_CUSTOM:      �ⲿʵ�ֺ��� */
#define LV_USE_STDLIB_SPRINTF   LV_STDLIB_BUILTIN

#define LV_STDINT_INCLUDE       <stdint.h>
#define LV_STDDEF_INCLUDE       <stddef.h>
#define LV_STDBOOL_INCLUDE      <stdbool.h>
#define LV_INTTYPES_INCLUDE     <inttypes.h>
#define LV_LIMITS_INCLUDE       <limits.h>
#define LV_STDARG_INCLUDE       <stdarg.h>

#if LV_USE_STDLIB_MALLOC == LV_STDLIB_BUILTIN
    /** `lv_malloc()` �����ڴ��С���ֽڣ���>= 2kB�� */
    #define LV_MEM_SIZE (64 * 1024U)          /**< [�ֽ�] */

    /** Ϊ `lv_malloc()` ��չ���ڴ�ش�С���ֽڣ� */
    #define LV_MEM_POOL_EXPAND_SIZE 0

    /** Ϊ�ڴ�����õ�ַ����������Ϊ��ͨ������䡣Ҳ�������ⲿ SRAM �С� */
    #define LV_MEM_ADR 0     /**< 0: δʹ�� */
    /* �ṩһ���ڴ����������ȡ LVGL ���ڴ�أ������ǵ�ַ������ my_malloc */
    #if LV_MEM_ADR == 0
        #undef LV_MEM_POOL_INCLUDE
        #undef LV_MEM_POOL_ALLOC
    #endif
#endif  /*LV_USE_STDLIB_MALLOC == LV_STDLIB_BUILTIN*/

/*====================
   HAL ����
 *====================*/

/** Ĭ����ʾˢ�¡������豸��ȡ�Ͷ����������� */
#define LV_DEF_REFR_PERIOD  4      /**< [����] */

/** Ĭ��ÿӢ����� (DPI)�����ڳ�ʼ��Ĭ�ϴ�С���粿���ߴ硢��ʽ�߾ࡣ
 * ����̫��Ҫ���ɵ������޸�Ĭ�ϴ�С�ͼ�ࣩ */
#define LV_DPI_DEF 130              /**< [����/Ӣ��] */

/*=================
 * ����ϵͳ
 *=================*/
/** ѡ��Ҫʹ�õĲ���ϵͳ����ѡѡ�
 * - LV_OS_NONE
 * - LV_OS_PTHREAD
 * - LV_OS_FREERTOS
 * - LV_OS_CMSIS_RTOS2
 * - LV_OS_RTTHREAD
 * - LV_OS_WINDOWS
 * - LV_OS_MQX
 * - LV_OS_SDL2
 * - LV_OS_CUSTOM */
#define LV_USE_OS   LV_OS_NONE

#if LV_USE_OS == LV_OS_CUSTOM
    #define LV_OS_CUSTOM_INCLUDE <stdint.h>
#endif
#if LV_USE_OS == LV_OS_FREERTOS
    /*
     * ʹ��ֱ��֪ͨ��� RTOS �����������ʹ���н������������ź������� 45% ��ʹ�ø��� RAM��
     * ����ֻ��һ�����������Ϊ�¼��Ľ�����ʱ������ʹ�� RTOS ����֪ͨ��
     */
    #define LV_USE_FREERTOS_TASK_NOTIFY 1
#endif

/*========================
 * ��Ⱦ����
 *========================*/

/** ������ͼ���ͼ����ж��뵽���ֽ��� */
#define LV_DRAW_BUF_STRIDE_ALIGN                1

/** �����ƻ���������ʼ��ַ���뵽���ֽ��� */
#define LV_DRAW_BUF_ALIGN                       4

/** ʹ�þ�����б任��
 * Ҫ��
 * - `LV_USE_MATRIX = 1`
 * - ��Ⱦ������Ҫ֧�� 3x3 ����任 */
#define LV_DRAW_TRANSFORM_USE_MATRIX            0

/* ����������� `style_opa < 255`������ `bg_opa`��`text_opa` �ȣ���� NORMAL ���ģʽ��
 * ������Ⱦǰ�Ỻ�嵽���򵥡�ͼ���С��������Էֿ黺�塣
 * ���任ͼ�㡱����������� `transform_angle/zoom`��ʹ�ø���Ļ��������޷��ֿ���� */

/** ��ͼ����Ŀ�껺������С */
#define LV_DRAW_LAYER_SIMPLE_BUF_SIZE    (24 * 1024)    /**< [�ֽ�] */

/* ���Ƽ򵥺ͱ任ͼ����������ڴ档
 * ����ӦΪ `LV_DRAW_LAYER_SIMPLE_BUF_SIZE` ��С�������Ҳʹ�ñ任ͼ�㣬
 * ��Ӧ�㹻�洢���Ĳ��������� x �߶� x 4 �����򣩡�
 * ����Ϊ 0 ��ʾ������ */
#define LV_DRAW_LAYER_MAX_MEMORY 0  /**< Ĭ�������� [�ֽ�] */

/** �����̵߳�ջ��С��
 * ע�⣺��������� FreeType �� ThorVG����������Ϊ 32KB ����� */
#define LV_DRAW_THREAD_STACK_SIZE    (8 * 1024)         /**< [�ֽ�] */

/** ����������߳����ȼ���
 *  ֵԽ�����ȼ�Խ�ߡ�
 *  ��ʹ�� lv_os.h �� lv_thread_prio_t ö�ٵ�ֵ��LV_THREAD_PRIO_LOWEST��
 *  LV_THREAD_PRIO_LOW��LV_THREAD_PRIO_MID��LV_THREAD_PRIO_HIGH��LV_THREAD_PRIO_HIGHEST
 *  ȷ�����ȼ�ֵ�����ϵͳ�ض������ȼ�������롣
 *  �����ȼ��������޵�ϵͳ�ϣ��� FreeRTOS�����ϸߵ�ֵ�������Ⱦ���ܣ������ܵ����������񼢶� */
#define LV_DRAW_THREAD_PRIO LV_THREAD_PRIO_HIGH

#define LV_USE_DRAW_SW 1
#if LV_USE_DRAW_SW == 1
    /*
     * ѡ���Խ�����ɫ��ʽ֧���Լ��ٴ����С��
     * ע�⣺ĳЩ�����ڲ�ʹ���ض���ɫ��ʽ�����磺
     * - ����ʹ�� RGB888
     * - ��͸���ȵ�λͼ����ʹ�� ARGB8888
     */
    #define LV_DRAW_SW_SUPPORT_RGB565       1
    #define LV_DRAW_SW_SUPPORT_RGB565_SWAPPED       0
    #define LV_DRAW_SW_SUPPORT_RGB565A8     1
    #define LV_DRAW_SW_SUPPORT_RGB888       0
    #define LV_DRAW_SW_SUPPORT_XRGB8888     0
    #define LV_DRAW_SW_SUPPORT_ARGB8888     0
    #define LV_DRAW_SW_SUPPORT_ARGB8888_PREMULTIPLIED 0
    #define LV_DRAW_SW_SUPPORT_L8           0
    #define LV_DRAW_SW_SUPPORT_AL88         0
    #define LV_DRAW_SW_SUPPORT_A8           1
    #define LV_DRAW_SW_SUPPORT_I1           0

    /* ��������ɫ��ʽ�У���������Ϊ����ص�������ֵ */
    #define LV_DRAW_SW_I1_LUM_THRESHOLD 127

    /** ���û��Ƶ�Ԫ������
     *  - > 1 ��Ҫ�� `LV_USE_OS` �����ò���ϵͳ
     *  - > 1 ��ʾ����߳̽�������Ⱦ��Ļ */
    #define LV_DRAW_SW_DRAW_UNIT_CNT    1

    /** ʹ�� Arm-2D ����������sw����Ⱦ */
    #define LV_USE_DRAW_ARM2D_SYNC      0

    /** ���ñ��� helium ������ */
    #define LV_USE_NATIVE_HELIUM_ASM    0

    /**
     * - 0: ʹ�ü���Ⱦ�������ܻ��Ƽ򵥾��Σ����䡢ͼ���ı���ֱ�ߣ�
     * - 1: ʹ�ø�����Ⱦ�������ܻ���Բ�ǡ���Ӱ��б�ߺͻ��� */
    #define LV_DRAW_SW_COMPLEX          1

    #if LV_DRAW_SW_COMPLEX == 1
        /** �������岿����Ӱ���㡣
         *  LV_DRAW_SW_SHADOW_CACHE_SIZE ��Ҫ����������Ӱ��С��������Ӱ��СΪ
         *  `shadow_width + radius`��������� LV_DRAW_SW_SHADOW_CACHE_SIZE^2 �� RAM ���� */
        #define LV_DRAW_SW_SHADOW_CACHE_SIZE 0

        /** ������󻺴��Բ����������
         *  Ϊ����ݱ��� 1/4 Բ��Բ�ܡ�
         *  ÿ��Բʹ�� `�뾶 * 4` �ֽڣ���õİ뾶�ᱻ���棩��
         *  - 0: ���û��� */
        #define LV_DRAW_SW_CIRCLE_CACHE_SIZE 4
    #endif

    #define  LV_USE_DRAW_SW_ASM     LV_DRAW_SW_ASM_NONE

    #if LV_USE_DRAW_SW_ASM == LV_DRAW_SW_ASM_CUSTOM
        #define  LV_DRAW_SW_ASM_CUSTOM_INCLUDE ""
    #endif

    /** �����������ø��ӽ�����ƣ��Ƕ����Խ��䡢���򽥱��׶�ν��� */
    #define LV_USE_DRAW_SW_COMPLEX_GRADIENTS    0

#endif

/* ʹ�� TSi �� NemaGFX */
#define LV_USE_NEMA_GFX 0

#if LV_USE_NEMA_GFX
    /** ѡ��Ҫʹ�õ� NemaGFX HAL����ѡѡ�
     * - LV_NEMA_HAL_CUSTOM
     * - LV_NEMA_HAL_STM32 */
    #define LV_USE_NEMA_HAL LV_NEMA_HAL_CUSTOM
    #if LV_USE_NEMA_HAL == LV_NEMA_HAL_STM32
        #define LV_NEMA_STM32_HAL_INCLUDE <stm32u5xx_hal.h>
    #endif

    /* ����ʸ��ͼ�β��������ڴ��� NemaVG ��ʱ���� */
    #define LV_USE_NEMA_VG 0
    #if LV_USE_NEMA_VG
        /* ����Ӧ�ó���ķֱ��ʣ����ڷ��� VG ��ػ����� */
        #define LV_NEMA_GFX_MAX_RESX 800
        #define LV_NEMA_GFX_MAX_RESY 600
    #endif
#endif

/** �� iMX RTxxx ƽ̨��ʹ�� NXP �� VG-Lite GPU */
#define LV_USE_DRAW_VGLITE 0

#if LV_USE_DRAW_VGLITE
    /** Ϊ��Ļ�ߴ� > 352 �������ý����λͼ����������ͨ���� */
    #define LV_USE_VGLITE_BLIT_SPLIT 0

    #if LV_USE_OS
        /** Ϊ VG-Lite ����ʹ�ö���Ļ����߳� */
        #define LV_USE_VGLITE_DRAW_THREAD 1

        #if LV_USE_VGLITE_DRAW_THREAD
            /** ���� VGLite �첽���ơ�����������ŶӲ�һ����ˢ�µ� GPU */
            #define LV_USE_VGLITE_DRAW_ASYNC 1
        #endif
    #endif

    /** ���� VGLite ���� */
    #define LV_USE_VGLITE_ASSERT 0

    /** ���� VGLite ������ */
    #define LV_USE_VGLITE_CHECK_ERROR 0
#endif

/** �� iMX RTxxx ƽ̨��ʹ�� NXP �� PXP */
#define LV_USE_PXP 0

#if LV_USE_PXP
    /** ʹ�� PXP ���л��� */
    #define LV_USE_DRAW_PXP 1

    /** ʹ�� PXP ��ת��ʾ */
    #define LV_USE_ROTATE_PXP 0

    #if LV_USE_DRAW_PXP && LV_USE_OS
        /** Ϊ PXP ����ʹ�ö���Ļ����߳� */
        #define LV_USE_PXP_DRAW_THREAD 1
    #endif

    /** ���� PXP ���� */
    #define LV_USE_PXP_ASSERT 0
#endif

/** �� MPU ƽ̨��ʹ�� NXP �� G2D */
#define LV_USE_DRAW_G2D 0

#if LV_USE_DRAW_G2D
    /** G2D ���Ƶ�Ԫ�ɴ洢����󻺳���������
     *  ����֡����������Դ */
    #define LV_G2D_HASH_TABLE_SIZE 50

    #if LV_USE_OS
        /** Ϊ G2D ����ʹ�ö���Ļ����߳� */
        #define LV_USE_G2D_DRAW_THREAD 1
    #endif

    /** ���� G2D ���� */
    #define LV_USE_G2D_ASSERT 0
#endif

/** �� RA ƽ̨��ʹ�� Renesas Dave2D */
#define LV_USE_DRAW_DAVE2D 0

/** ʹ�û���� SDL �������л��� */
#define LV_USE_DRAW_SDL 0

/** ʹ�� VG-Lite GPU */
#define LV_USE_DRAW_VG_LITE 0

#if LV_USE_DRAW_VG_LITE
    /** ���� VG-Lite �Զ����ⲿ 'gpu_init()' ���� */
    #define LV_VG_LITE_USE_GPU_INIT 0

    /** ���� VG-Lite ���� */
    #define LV_VG_LITE_USE_ASSERT 0

    /** VG-Lite ˢ���ύ������ֵ��GPU ����������������ô��������� */
    #define LV_VG_LITE_FLUSH_MAX_COUNT 8

    /** ���ñ߿���ģ����Ӱ��
     *  ע�⣺ͨ����������ܣ�������֤��������ͬ����Ⱦ���� */
    #define LV_VG_LITE_USE_BOX_SHADOW 1

    /** VG-Lite ������󻺴�������
     *  @ע�� ��������ͼ����ڴ�ʹ����Ϊ 4K �ֽ� */
    #define LV_VG_LITE_GRAD_CACHE_CNT 32

    /** VG-Lite �����󻺴����� */
    #define LV_VG_LITE_STROKE_CACHE_CNT 32
#endif

/** ʹ�� STM32 DMA2D ���ٻ�ϡ����Ȳ��� */
#define LV_USE_DRAW_DMA2D 0

#if LV_USE_DRAW_DMA2D
    #define LV_DRAW_DMA2D_HAL_INCLUDE "stm32h7xx_hal.h"

    /* ������ã��û���Ҫ���յ� DMA2D ȫ���ж�ʱ���� `lv_draw_dma2d_transfer_complete_interrupt_handler` */
    #define LV_USE_DRAW_DMA2D_INTERRUPT 0
#endif

/** ʹ�û���� OpenGLES �������л��� */
#define LV_USE_DRAW_OPENGLES 0

/*=======================
 * ��������
 *=======================*/

/*-------------
 * ��־
 *-----------*/

/** ������־ģ�� */
#define LV_USE_LOG 0
#if LV_USE_LOG
    /** ��ֵ����Ϊ������־��ϸ����֮һ��
     *  - LV_LOG_LEVEL_TRACE    ��¼��ϸ��Ϣ
     *  - LV_LOG_LEVEL_INFO     ��¼��Ҫ�¼�
     *  - LV_LOG_LEVEL_WARN     ��¼��ϣ��������δ������������
     *  - LV_LOG_LEVEL_ERROR    ����¼�ؼ����⣨ϵͳ����ʧ��ʱ��
     *  - LV_LOG_LEVEL_USER     ����¼�û����ӵ��Զ�����־��Ϣ
     *  - LV_LOG_LEVEL_NONE     ����¼�κ����� */
    #define LV_LOG_LEVEL LV_LOG_LEVEL_WARN

    /** - 1: ʹ�� 'printf' ��ӡ��־
     *  - 0: �û���Ҫʹ�� `lv_log_register_print_cb()` ע��ص� */
    #define LV_LOG_PRINTF 0

    /** ���ô�ӡ��־�Ļص���
     *  ���� `my_print`��ԭ��ӦΪ `void my_print(lv_log_level_t level, const char * buf)`
     *  �ɱ� `lv_log_register_print_cb` ���� */
    //#define LV_LOG_PRINT_CB

    /** - 1: ���ô�ӡʱ���
     *  - 0: ���ô�ӡʱ��� */
    #define LV_LOG_USE_TIMESTAMP 1

    /** - 1: ��ӡ��־���ļ������к�
     *  - 0: ����ӡ��־���ļ������к� */
    #define LV_LOG_USE_FILE_LINE 1

    /* �ڲ���������־��ģ��������/���� LV_LOG_TRACE */
    #define LV_LOG_TRACE_MEM        1   /**< ���ڴ����������/���ø�����־ */
    #define LV_LOG_TRACE_TIMER      1   /**< �ڶ�ʱ������������/���ø�����־ */
    #define LV_LOG_TRACE_INDEV      1   /**< �������豸����������/���ø�����־ */
    #define LV_LOG_TRACE_DISP_REFR  1   /**< ����ʾ�ػ����������/���ø�����־ */
    #define LV_LOG_TRACE_EVENT      1   /**< ���¼��ַ��߼�������/���ø�����־ */
    #define LV_LOG_TRACE_OBJ_CREATE 1   /**< �ڶ��󴴽�������/���ø�����־������ `obj` ������ÿ�������� */
    #define LV_LOG_TRACE_LAYOUT     1   /**< �� Flex �� Grid ���ֲ���������/���ø�����־ */
    #define LV_LOG_TRACE_ANIM       1   /**< �ڶ����߼�������/���ø�����־ */
    #define LV_LOG_TRACE_CACHE      1   /**< �ڻ������������/���ø�����־ */
#endif  /*LV_USE_LOG*/

/*-------------
 * ����
 *-----------*/

/* �������ʧ�ܻ�����Ч���ݣ����ö���ʧ�ܡ�
 * ��������� LV_USE_LOG������ʧ��ʱ��ӡ������Ϣ */
#define LV_USE_ASSERT_NULL          1   /**< �������Ƿ�Ϊ NULL���ǳ��죬�Ƽ��� */
#define LV_USE_ASSERT_MALLOC        1   /**< ����ڴ��Ƿ�ɹ����䣨�ǳ��죬�Ƽ��� */
#define LV_USE_ASSERT_STYLE         0   /**< �����ʽ�Ƿ���ȷ��ʼ�����ǳ��죬�Ƽ��� */
#define LV_USE_ASSERT_MEM_INTEGRITY 0   /**< �ڹؼ��������� `lv_mem` �������ԣ����� */
#define LV_USE_ASSERT_OBJ           0   /**< ����������ͺʹ����ԣ�����δɾ���������� */

/** ���Է���ʱ�����Զ��崦�������������� MCU */
#define LV_ASSERT_HANDLER_INCLUDE <stdint.h>
#define LV_ASSERT_HANDLER while(1);     /**< Ĭ����ͣ */

/*-------------
 * ����
 *-----------*/

/** 1: ���ػ������ϻ��������ɫ�ľ��� */
#define LV_USE_REFR_DEBUG 0

/** 1: Ϊ ARGB ͼ����ƺ�ɫ���ǲ㣬Ϊ RGB ͼ�������ɫ���ǲ� */
#define LV_USE_LAYER_DEBUG 0

/** 1: Ϊ��������������Ϊ��
 *  - Ϊÿ�����Ƶ�Ԫ��������Ʋ�ͬ��ɫ�ĸ��ǲ�
 *  - �ڰ�ɫ�����ϻ��ƻ��Ƶ�Ԫ��������
 *  - ����ͼ�㣬�ں�ɫ�����ϻ��ƻ��Ƶ�Ԫ�������� */
#define LV_USE_PARALLEL_DRAW_DEBUG 0

/*-------------
 * ����
 *-----------*/

#define LV_ENABLE_GLOBAL_CUSTOM 0
#if LV_ENABLE_GLOBAL_CUSTOM
    /** �Զ��� 'lv_global' ����Ҫ������ͷ�ļ� */
    #define LV_GLOBAL_CUSTOM_INCLUDE <stdint.h>
#endif

/** Ĭ�ϻ����С���ֽڣ���
 *  ��ͼ����������� `lv_lodepng`���������ڴ��б��������ͼ��
 *  �����Сδ����Ϊ 0����������ʱ������������ʧ�ܡ�
 *  �����СΪ 0������û��湦�ܣ��������ڴ���ʹ�ú������ͷ� */
#define LV_CACHE_DEF_SIZE       0

/** Ĭ��ͼ��ͷ������Ŀ�����������ڴ洢ͼ��ͷ
 *  ��Ҫ�߼����� `LV_CACHE_DEF_SIZE`��������ͼ��ͷ */
#define LV_IMAGE_HEADER_CACHE_DEF_CNT 0

/** ÿ������������ɫ�����������Ӵ�ֵ����������ɫ�ꡣ
 *  ÿ�������ɫ������ (sizeof(lv_color_t) + 1) �ֽ� */
#define LV_GRADIENT_MAX_STOPS   2

/** ������ɫ��Ϻ��������뷽ʽ��GPU �����Բ�ͬ��ʽ������ɫ��ϣ���ϣ�
 *  - 0:   ��������
 *  - 64:  �� x.75 ��������
 *  - 128: ��һ����������
 *  - 192: �� x.25 ��������
 *  - 254: �������� */
#define LV_COLOR_MIX_ROUND_OFS  0

/** ��ÿ�� `lv_obj_t` ������ 2 �� 32 λ�����Լ��ٻ�ȡ��ʽ���� */
#define LV_OBJ_STYLE_CACHE      0

/** �� `lv_obj_t` ������ `id` �ֶ� */
#define LV_USE_OBJ_ID           0

/** ���ò�������֧�� */
#define LV_USE_OBJ_NAME         0

/** ���� obj ʱ�Զ����� ID */
#define LV_OBJ_ID_AUTO_ASSIGN   LV_USE_OBJ_ID

/** ʹ�����õ� obj ID ����������
* - lv_obj_assign_id:       ��������ʱ���á���ÿ��������ʹ�õ����ļ�������Ϊ ID
* - lv_obj_id_compare:      �Ƚ� ID �Ծ����Ƿ�ƥ�������ֵ
* - lv_obj_stringify_id:    �����ַ������ı�ʶ�������� "button3"
* - lv_obj_free_id:         ��ִ���κβ�������Ϊ ID û���ڴ����
* ����ʱ���û���Ҫʵ����Щ���� */
#define LV_USE_OBJ_ID_BUILTIN   1

/** ʹ�� obj ��������/��ȡ API */
#define LV_USE_OBJ_PROPERTY 0

/** ������������֧�� */
#define LV_USE_OBJ_PROPERTY_NAME 1

/* ʹ�� VG-Lite ģ����
 * - Ҫ��LV_USE_THORVG_INTERNAL �� LV_USE_THORVG_EXTERNAL */
#define LV_USE_VG_LITE_THORVG  0

#if LV_USE_VG_LITE_THORVG
    /** ���� LVGL ���õĻ��ģʽ֧�� */
    #define LV_VG_LITE_THORVG_LVGL_BLEND_SUPPORT 0

    /** ���� YUV ��ɫ��ʽ֧�� */
    #define LV_VG_LITE_THORVG_YUV_SUPPORT 0

    /** �������Խ�����չ֧�� */
    #define LV_VG_LITE_THORVG_LINEAR_GRADIENT_EXT_SUPPORT 0

    /** ���� 16 ���ض��� */
    #define LV_VG_LITE_THORVG_16PIXELS_ALIGN 1

    /** ��������ַ���� */
    #define LV_VG_LITE_THORVG_BUF_ADDR_ALIGN 64

    /** ���ö��߳���Ⱦ */
    #define LV_VG_LITE_THORVG_THREAD_RENDER 0
#endif

/* ��������ʶ���ܣ���㴥�أ� */
/* ����ʶ����Ҫʹ�ø����� */
#define LV_USE_GESTURE_RECOGNITION 0

/*=====================
 * ����������
 *====================*/

/** ���ڴ��ϵͳ����Ϊ 1 */
#define LV_BIG_ENDIAN_SYSTEM 0

/** Ϊ `lv_tick_inc` ���������Զ������� */
#define LV_ATTRIBUTE_TICK_INC

/** Ϊ `lv_timer_handler` ���������Զ������� */
#define LV_ATTRIBUTE_TIMER_HANDLER

/** Ϊ `lv_display_flush_ready` ���������Զ������� */
#define LV_ATTRIBUTE_FLUSH_READY

/** �� VG_LITE ���������뵽���ֽ���
 *  @ע�� vglite_src_buf_aligned() ʹ�ô�ֵ��֤���ݵĻ�����ָ��Ķ��� */
#define LV_ATTRIBUTE_MEM_ALIGN_SIZE 1

/** ����Ҫ�ڴ����ĵط����ӣ�ʹ�� -Os ʱĬ�����ݿ���δ����߽磩
 *  ���� __attribute__((aligned(4))) */
#define LV_ATTRIBUTE_MEM_ALIGN

/** ���ڱ�Ǵ��ͳ�����������ԣ���������λͼ */
#define LV_ATTRIBUTE_LARGE_CONST

/** RAM �д����������ı�����ǰ׺ */
#define LV_ATTRIBUTE_LARGE_RAM_ARRAY

/** �����ܹؼ��������������ڴ棨���� RAM�� */
#define LV_ATTRIBUTE_FAST_MEM

/** �����ͳ����������󶨡��˺����� LV_<CONST> ��ʽ�ĳ�����
 *  ��Щ����ҲӦ������ LVGL �� API���� MicroPython���� */
#define LV_EXPORT_CONST_INT(int_value) struct _silence_gcc_warning  /**< Ĭ��ֵ�����ڷ�ֹ GCC ���� */

/** ����ȫ���ⲿ���ݵ�ǰ׺ */
#define LV_ATTRIBUTE_EXTERN_DATA

/** ʹ�� `float` ��Ϊ `lv_value_precise_t` */
#define LV_USE_FLOAT            0

/** ���þ���֧��
 *  - Ҫ�� `LV_USE_FLOAT = 1` */
#define LV_USE_MATRIX           0

/** Ĭ���� `lvgl.h` �а��� `lvgl_private.h` �Է����ڲ����ݺͺ��� */
#ifndef LV_USE_PRIVATE_API
    #define LV_USE_PRIVATE_API  0
#endif

/*==================
 *   ����ʹ��
 *===================*/

/* ���� ASCII ��Χ��һЩ���ŵ� Montserrat ���壬ʹ�� bpp = 4
 * https://fonts.google.com/specimen/Montserrat */
#define LV_FONT_MONTSERRAT_8  0
#define LV_FONT_MONTSERRAT_10 0
#define LV_FONT_MONTSERRAT_12 0
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 0
#define LV_FONT_MONTSERRAT_18 0
#define LV_FONT_MONTSERRAT_20 0
#define LV_FONT_MONTSERRAT_22 0
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_26 0
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_30 0
#define LV_FONT_MONTSERRAT_32 0
#define LV_FONT_MONTSERRAT_34 0
#define LV_FONT_MONTSERRAT_36 0
#define LV_FONT_MONTSERRAT_38 0
#define LV_FONT_MONTSERRAT_40 0
#define LV_FONT_MONTSERRAT_42 0
#define LV_FONT_MONTSERRAT_44 0
#define LV_FONT_MONTSERRAT_46 0
#define LV_FONT_MONTSERRAT_48 0

/* ��ʾ���⹦�� */
#define LV_FONT_MONTSERRAT_28_COMPRESSED    0  /**< bpp = 3 */
#define LV_FONT_DEJAVU_16_PERSIAN_HEBREW    0  /**< ϣ��������������˹����ĸ����������ʽ */
#define LV_FONT_SIMSUN_14_CJK               0  /**< 1000 ����õ� CJK ���� */
#define LV_FONT_SIMSUN_16_CJK               0  /**< 1000 ����õ� CJK ���� */
#define LV_FONT_SOURCE_HAN_SANS_SC_14_CJK   0  /**< 1338 ����õ� CJK ���� */
#define LV_FONT_SOURCE_HAN_SANS_SC_16_CJK   0  /**< 1338 ����õ� CJK ���� */

/** ���������ĵȿ����� */
#define LV_FONT_UNSCII_8  0
#define LV_FONT_UNSCII_16 0

/** ���ڴ˴������Զ�������
 *
 *  �����Խ������κ���������Ĭ�����壬���ǽ�ȫ�ֿ���
 *  ʾ����
 *
 *  @code
 *  #define LV_FONT_CUSTOM_DECLARE   LV_FONT_DECLARE(my_font_1) LV_FONT_DECLARE(my_font_2)
 *  @endcode
 */
#define LV_FONT_CUSTOM_DECLARE

/** ʼ������Ĭ������ */
#define LV_FONT_DEFAULT &lv_font_montserrat_14

/** ���ô������������/����д����ַ�������
 *  ����ȡ���������С������� bpp
 *  ���������Ҫ������������������ */
#define LV_FONT_FMT_TXT_LARGE 0

/** ����/���ö�ѹ�������֧�� */
#define LV_USE_FONT_COMPRESSED 0

/** ���Ҳ�����������ʱ���û���ռλ�� */
#define LV_USE_FONT_PLACEHOLDER 1

/*=================
 *  �ı�����
 *=================*/

/**
 * Ϊ�ַ���ѡ���ַ�����
 * ���� IDE ��༭��Ӧʹ����ͬ���ַ�����
 * - LV_TXT_ENC_UTF8
 * - LV_TXT_ENC_ASCII
 */
#define LV_TXT_ENC LV_TXT_ENC_UTF8

/** ��Ⱦ�ı��ַ���ʱ������Щ�ַ��ϻ��� */
#define LV_TXT_BREAK_CHARS " ,.;:-_)]}"

/** ������ʳ�������Ϊ��ֵ�����ڡ�����ʡ���λ�û���
 *  Ҫ���ã�������ֵ <= 0 */
#define LV_TXT_LINE_BREAK_LONG_LEN 0

/** �ڻ���ǰ����һ���ϵĳ����ʵ���С�ַ���
 *  ȡ���� LV_TXT_LINE_BREAK_LONG_LEN */
#define LV_TXT_LINE_BREAK_LONG_PRE_MIN_LEN 3

/** �ڻ��к����һ���ϵĳ����ʵ���С�ַ���
 *  ȡ���� LV_TXT_LINE_BREAK_LONG_LEN */
#define LV_TXT_LINE_BREAK_LONG_POST_MIN_LEN 3

/** ֧��˫���ı���������ϴ����Һʹ��ҵ�����ı�
 *  ���򽫸��� Unicode ˫���㷨������
 *  https://www.w3.org/International/articles/inline-bidi-markup/uba-basics */
#define LV_USE_BIDI 0
#if LV_USE_BIDI
    /* ����Ĭ�Ϸ���֧�ֵ�ֵ��
    * `LV_BASE_DIR_LTR` ������
    * `LV_BASE_DIR_RTL` ���ҵ���
    * `LV_BASE_DIR_AUTO` ����ı��������� */
    #define LV_BIDI_BASE_DIR_DEF LV_BASE_DIR_AUTO
#endif

/** ���ð�������/��˹�ﴦ��
 *  ����Щ�����У��ַ�Ӧ���������ı��е�λ���滻Ϊ��һ����ʽ */
#define LV_USE_ARABIC_PERSIAN_CHARS 0

/* �����ź��ı�������ɫ�Ŀ����ַ� */
#define LV_TXT_COLOR_CMD "#"

/*==================
 * ����
 *================*/
/* �����ĵ���https://docs.lvgl.io/master/details/widgets/index.html */

/** 1: ʹ��Щ�����ڴ���ʱ���Ĭ��ֵ
 *  - lv_buttonmatrix_t: ��ȡĬ��ӳ�䣺{"Btn1", "Btn2", "Btn3", "\n", "Btn4", "Btn5", ""}����������ӳ��
 *  - lv_checkbox_t    : �ַ�����ǩ����Ϊ "Check box"����������Ϊ���ַ���
 *  - lv_dropdown_t    : ѡ������Ϊ "Option 1", "Option 2", "Option 3"����������ֵ
 *  - lv_roller_t      : ѡ������Ϊ "Option 1", "Option 2", "Option 3", "Option 4", "Option 5"����������ֵ
 *  - lv_label_t       : �ı�����Ϊ "Text"������Ϊ���ַ���
 * */
#define LV_WIDGETS_HAS_DEFAULT_VALUE  0

/* CabinetGuard UI widgets trimmed: keep label/button/switch/arc/bar only */

#define LV_USE_ANIMIMG    0

#define LV_USE_ARC        1

#define LV_USE_BAR        1

#define LV_USE_BUTTON        1

#define LV_USE_BUTTONMATRIX  0

#define LV_USE_CALENDAR   0
#if LV_USE_CALENDAR
    #define LV_CALENDAR_WEEK_STARTS_MONDAY 0
    #if LV_CALENDAR_WEEK_STARTS_MONDAY
        #define LV_CALENDAR_DEFAULT_DAY_NAMES {"Mo", "Tu", "We", "Th", "Fr", "Sa", "Su"}
    #else
        #define LV_CALENDAR_DEFAULT_DAY_NAMES {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"}
    #endif

    #define LV_CALENDAR_DEFAULT_MONTH_NAMES {"January", "February", "March",  "April", "May",  "June", "July", "August", "September", "October", "November", "December"}
    #define LV_USE_CALENDAR_HEADER_ARROW 1
    #define LV_USE_CALENDAR_HEADER_DROPDOWN 1
    #define LV_USE_CALENDAR_CHINESE 0
#endif  /*LV_USE_CALENDAR*/

#define LV_USE_CANVAS     0

#define LV_USE_CHART      0

#define LV_USE_CHECKBOX   0

#define LV_USE_DROPDOWN   0   /**< Ҫ��lv_label */

#define LV_USE_IMAGE      0   /**< Ҫ��lv_label */

#define LV_USE_IMAGEBUTTON     0

#define LV_USE_KEYBOARD   0

#define LV_USE_LABEL      1
#if LV_USE_LABEL
    #define LV_LABEL_TEXT_SELECTION 0   /**< ����ѡ���ǩ�ı� */
    #define LV_LABEL_LONG_TXT_HINT 0    /**< �ڱ�ǩ�д洢������Ϣ�Լ��ٻ��Ƴ����ı� */
    #define LV_LABEL_WAIT_CHAR_COUNT 3  /**< �ȴ��ַ������� */
#endif

#define LV_USE_LED        0

#define LV_USE_LINE       0

#define LV_USE_LIST       0

#define LV_USE_LOTTIE     0  /**< Ҫ��lv_canvas, thorvg */

#define LV_USE_MENU       0

#define LV_USE_MSGBOX     0

#define LV_USE_ROLLER     0   /**< Ҫ��lv_label */

#define LV_USE_SCALE      0

#define LV_USE_SLIDER     0   /**< Ҫ��lv_bar */

#define LV_USE_SPAN       0
#if LV_USE_SPAN
    /** һ���ı��ɰ�������������� span ������ */
    #define LV_SPAN_SNIPPET_STACK_SIZE 64
#endif

#define LV_USE_SPINBOX    0

#define LV_USE_SPINNER    0

#define LV_USE_SWITCH     1

#define LV_USE_TABLE      0

#define LV_USE_TABVIEW    0

#define LV_USE_TEXTAREA   0   /**< Ҫ��lv_label */
#if LV_USE_TEXTAREA != 0
    #define LV_TEXTAREA_DEF_PWD_SHOW_TIME 1500    /**< [����] */
#endif

#define LV_USE_TILEVIEW   0

#define LV_USE_WIN        0

#define LV_USE_3DTEXTURE  0

/*==================
 * ����
 *==================*/
/* �����ĵ���https://docs.lvgl.io/master/details/common-widget-features/styles/style.html#themes */

/** һ���򵥡���ɫ�ҷǳ����������� */
#define LV_USE_THEME_DEFAULT 1
#if LV_USE_THEME_DEFAULT
    /** 0: ��ģʽ��1: ��ģʽ */
    #define LV_THEME_DEFAULT_DARK 0

    /** 1: ���ð���ʱ�Ŵ� */
    #define LV_THEME_DEFAULT_GROW 0

    /** Ĭ�Ϲ���ʱ�䣨���룩 */
    #define LV_THEME_DEFAULT_TRANSITION_TIME 0
#endif /*LV_USE_THEME_DEFAULT*/

/** һ���ǳ��򵥵����⣬���Զ��������������� */
#define LV_USE_THEME_SIMPLE 0

/** רΪ��ɫ��ʾ��Ƶ����� */
#define LV_USE_THEME_MONO 0

/*==================
 * ����
 *==================*/
/* �����ĵ���https://docs.lvgl.io/master/details/common-widget-features/layouts/index.html */

/** ���� CSS �� Flexbox �Ĳ��� */
#define LV_USE_FLEX 0

/** ���� CSS �� Grid �Ĳ��� */
#define LV_USE_GRID 0

/*====================
 * ��������
 *====================*/
/* ���ĵ���https://docs.lvgl.io/master/details/libs/index.html */

/* ͨ�� API ���ļ�ϵͳ�ӿ� */

/** ����Ĭ����������ĸ�������ļ�·��������������ǰ׺
 *  �й����ʹ��������������ʶ����ĸ���ĵ���
 *  https://docs.lvgl.io/master/details/main-modules/fs.html#lv-fs-identifier-letters */
#define LV_FS_DEFAULT_DRIVER_LETTER '\0'

/** fopen, fread �ȵ� API */
#define LV_USE_FS_STDIO 0
#if LV_USE_FS_STDIO
    #define LV_FS_STDIO_LETTER '\0'     /**< Ϊ�����������ô�д��������ʶ����ĸ������ 'A'�� */
    #define LV_FS_STDIO_PATH ""         /**< ���ù���Ŀ¼���ļ�/Ŀ¼·�������ӵ���� */
    #define LV_FS_STDIO_CACHE_SIZE 0    /**< >0 ���� lv_fs_read() �л�����ֽ��� */
#endif

/** open, read �ȵ� API */
#define LV_USE_FS_POSIX 0
#if LV_USE_FS_POSIX
    #define LV_FS_POSIX_LETTER '\0'     /**< Ϊ�����������ô�д��������ʶ����ĸ������ 'A'�� */
    #define LV_FS_POSIX_PATH ""         /**< ���ù���Ŀ¼���ļ�/Ŀ¼·�������ӵ���� */
    #define LV_FS_POSIX_CACHE_SIZE 0    /**< >0 ���� lv_fs_read() �л�����ֽ��� */
#endif

/** CreateFile, ReadFile �ȵ� API */
#define LV_USE_FS_WIN32 0
#if LV_USE_FS_WIN32
    #define LV_FS_WIN32_LETTER '\0'     /**< Ϊ�����������ô�д��������ʶ����ĸ������ 'A'�� */
    #define LV_FS_WIN32_PATH ""         /**< ���ù���Ŀ¼���ļ�/Ŀ¼·�������ӵ���� */
    #define LV_FS_WIN32_CACHE_SIZE 0    /**< >0 ���� lv_fs_read() �л�����ֽ��� */
#endif

/** FATFS �� API����Ҫ�������ӣ���ʹ�� f_open, f_read �� */
#define LV_USE_FS_FATFS 0
#if LV_USE_FS_FATFS
    #define LV_FS_FATFS_LETTER '\0'     /**< Ϊ�����������ô�д��������ʶ����ĸ������ 'A'�� */
    #define LV_FS_FATFS_PATH ""         /**< ���ù���Ŀ¼���ļ�/Ŀ¼·�������ӵ���� */
    #define LV_FS_FATFS_CACHE_SIZE 0    /**< >0 ���� lv_fs_read() �л�����ֽ��� */
#endif

/** �ڴ�ӳ���ļ����ʵ� API */
#define LV_USE_FS_MEMFS 0
#if LV_USE_FS_MEMFS
    #define LV_FS_MEMFS_LETTER '\0'     /**< Ϊ�����������ô�д��������ʶ����ĸ������ 'A'�� */
#endif

/** LittleFs �� API */
#define LV_USE_FS_LITTLEFS 0
#if LV_USE_FS_LITTLEFS
    #define LV_FS_LITTLEFS_LETTER '\0'  /**< Ϊ�����������ô�д��������ʶ����ĸ������ 'A'�� */
    #define LV_FS_LITTLEFS_PATH ""      /**< ���ù���Ŀ¼���ļ�/Ŀ¼·�������ӵ���� */
#endif

/** Arduino LittleFs �� API */
#define LV_USE_FS_ARDUINO_ESP_LITTLEFS 0
#if LV_USE_FS_ARDUINO_ESP_LITTLEFS
    #define LV_FS_ARDUINO_ESP_LITTLEFS_LETTER '\0'  /**< Ϊ�����������ô�д��������ʶ����ĸ������ 'A'�� */
    #define LV_FS_ARDUINO_ESP_LITTLEFS_PATH ""      /**< ���ù���Ŀ¼���ļ�/Ŀ¼·�������ӵ���� */
#endif

/** Arduino Sd �� API */
#define LV_USE_FS_ARDUINO_SD 0
#if LV_USE_FS_ARDUINO_SD
    #define LV_FS_ARDUINO_SD_LETTER '\0'  /**< Ϊ�����������ô�д��������ʶ����ĸ������ 'A'�� */
    #define LV_FS_ARDUINO_SD_PATH ""      /**< ���ù���Ŀ¼���ļ�/Ŀ¼·�������ӵ���� */
#endif

/** UEFI �� API */
#define LV_USE_FS_UEFI 0
#if LV_USE_FS_UEFI
    #define LV_FS_UEFI_LETTER '\0'      /**< Ϊ�����������ô�д��������ʶ����ĸ������ 'A'�� */
#endif

/** LODEPNG ����� */
#define LV_USE_LODEPNG 0

/** PNG ����� (libpng) */
#define LV_USE_LIBPNG 0

/** BMP ����� */
#define LV_USE_BMP 0

/** JPG + ��� JPG �����
 *  ��� JPG ��ΪǶ��ʽϵͳ�Ż����Զ����ʽ */
#define LV_USE_TJPGD 0

/** libjpeg-turbo �����
 *  - ֧�������� JPEG �淶�͸����� JPEG ���� */
#define LV_USE_LIBJPEG_TURBO 0

/** GIF ����� */
#define LV_USE_GIF 0
#if LV_USE_GIF
    /** GIF ������� */
    #define LV_GIF_CACHE_DECODE_DATA 0
#endif


/** �� bin ͼ����뵽 RAM */
#define LV_BIN_DECODER_RAM_LOAD 0

/** RLE ��ѹ���� */
#define LV_USE_RLE 0

/** ��ά��� */
#define LV_USE_QRCODE 0

/** ������� */
#define LV_USE_BARCODE 0

/** FreeType �� */
#define LV_USE_FREETYPE 0
#if LV_USE_FREETYPE
    /** �� FreeType ʹ�� LVGL �ڴ���ļ���ֲ */
    #define LV_FREETYPE_USE_LVGL_PORT 0

    /** FreeType �л�����������������ɻ������������
     *  ֵԽ�ߣ�ʹ�õ��ڴ�Խ�� */
    #define LV_FREETYPE_CACHE_FT_GLYPH_CNT 256
#endif

/** ���� TTF ������ */
#define LV_USE_TINY_TTF 0
#if LV_USE_TINY_TTF
    /* ���ô��ļ����� TTF ���� */
    #define LV_TINY_TTF_FILE_SUPPORT 0
    #define LV_TINY_TTF_CACHE_GLYPH_CNT 256
#endif

/** Rlottie �� */
#define LV_USE_RLOTTIE 0

/** ����ʸ��ͼ�� API
 *  - Ҫ�� `LV_USE_MATRIX = 1` */
#define LV_USE_VECTOR_GRAPHIC  0

/** �� src/libs �ļ������� ThorVG��ʸ��ͼ�ο⣩ */
#define LV_USE_THORVG_INTERNAL 0

/** ͨ������ ThorVG �Ѱ�װ�����ӵ���Ŀ������ */
#define LV_USE_THORVG_EXTERNAL 0

/** ʹ�� lvgl ���õ� LZ4 �� */
#define LV_USE_LZ4_INTERNAL  0

/** ʹ���ⲿ LZ4 �� */
#define LV_USE_LZ4_EXTERNAL  0

/*SVG ��
 *  - Ҫ�� `LV_USE_VECTOR_GRAPHIC = 1` */
#define LV_USE_SVG 0
#define LV_USE_SVG_ANIMATION 0
#define LV_USE_SVG_DEBUG 0

/** ����ͼ�����Ͳ�����Ƶ�� FFmpeg ��
 *  ֧��������Ҫͼ���ʽ����˲�Ҫ������ͼ�������һ������ */
#define LV_USE_FFMPEG 0
#if LV_USE_FFMPEG
    /** ��������Ϣת���� stderr */
    #define LV_FFMPEG_DUMP_FORMAT 0
    /** �� FFmpeg ������������ʹ�� lvgl �ļ�·��
     *  ���ô˹��ܺ��޷��� URL
     *  ע�⣺FFmpeg ͼ���������ʼ��ʹ�� lvgl �ļ�ϵͳ */
    #define LV_FFMPEG_PLAYER_USE_LV_FS 0
#endif

/*==================
 * ����
 *==================*/
/* ���²�����Ŀ���ĵ���https://docs.lvgl.io/master/details/auxiliary-modules/index.html */

/** 1: ����Ϊ����������յ� API */
#define LV_USE_SNAPSHOT 0

/** 1: ����ϵͳ��������� */
#define LV_USE_SYSMON   0
#if LV_USE_SYSMON
    /** ��ȡ���аٷֱȡ����� uint32_t my_get_idle(void); */
    #define LV_SYSMON_GET_IDLE lv_os_get_idle_percent

    /** 1: ��ʾ CPU ʹ���ʺ� FPS ����
     *  - Ҫ�� `LV_USE_SYSMON = 1` */
    #define LV_USE_PERF_MONITOR 0
    #if LV_USE_PERF_MONITOR
        #define LV_USE_PERF_MONITOR_POS LV_ALIGN_BOTTOM_RIGHT

        /** 0: ����Ļ����ʾ�������ݣ�1: ʹ����־��ӡ�������� */
        #define LV_USE_PERF_MONITOR_LOG_MODE 0
    #endif

    /** 1: ��ʾ�����ڴ���ڴ���Ƭ
     *     - Ҫ�� `LV_USE_STDLIB_MALLOC = LV_STDLIB_BUILTIN`
     *     - Ҫ�� `LV_USE_SYSMON = 1`*/
    #define LV_USE_MEM_MONITOR 0
    #if LV_USE_MEM_MONITOR
        #define LV_USE_MEM_MONITOR_POS LV_ALIGN_BOTTOM_LEFT
    #endif
#endif /*LV_USE_SYSMON*/

/** 1: ��������ʱ���ܷ����� */
#define LV_USE_PROFILER 0
#if LV_USE_PROFILER
    /** 1: �������÷����� */
    #define LV_USE_PROFILER_BUILTIN 1
    #if LV_USE_PROFILER_BUILTIN
        /** Ĭ�Ϸ��������ٻ�������С */
        #define LV_PROFILER_BUILTIN_BUF_SIZE (16 * 1024)     /**< [�ֽ�] */
        #define LV_PROFILER_BUILTIN_DEFAULT_ENABLE 1
    #endif

    /** ������Ҫ������ͷ�ļ� */
    #define LV_PROFILER_INCLUDE "lvgl/src/misc/lv_profiler_builtin.h"

    /** ��������㺯�� */
    #define LV_PROFILER_BEGIN    LV_PROFILER_BUILTIN_BEGIN

    /** �������յ㺯�� */
    #define LV_PROFILER_END      LV_PROFILER_BUILTIN_END

    /** ���Զ����ǩ�ķ�������㺯�� */
    #define LV_PROFILER_BEGIN_TAG LV_PROFILER_BUILTIN_BEGIN_TAG

    /** ���Զ����ǩ�ķ������յ㺯�� */
    #define LV_PROFILER_END_TAG   LV_PROFILER_BUILTIN_END_TAG

    /* ���ò��ַ����� */
    #define LV_PROFILER_LAYOUT 1

    /* ������ʾˢ�·����� */
    #define LV_PROFILER_REFR 1

    /* ���û��Ʒ����� */
    #define LV_PROFILER_DRAW 1

    /* ���������豸������ */
    #define LV_PROFILER_INDEV 1

    /* ���ý����������� */
    #define LV_PROFILER_DECODER 1

    /* ������������� */
    #define LV_PROFILER_FONT 1

    /* �����ļ�ϵͳ������ */
    #define LV_PROFILER_FS 1

    /* ������ʽ������ */
    #define LV_PROFILER_STYLE 0

    /* ���ö�ʱ�������� */
    #define LV_PROFILER_TIMER 1

    /* ���û�������� */
    #define LV_PROFILER_CACHE 1

    /* �����¼������� */
    #define LV_PROFILER_EVENT 1
#endif

/** 1: ���� Monkey ���� */
#define LV_USE_MONKEY 0

/** 1: �������񵼺� */
#define LV_USE_GRIDNAV 0

/** 1: ���� `lv_obj` Ƭ���߼� */
#define LV_USE_FRAGMENT 0

/** 1: ֧���ڱ�ǩ�� span ������ʹ��ͼ����Ϊ���� */
#define LV_USE_IMGFONT 0

/** 1: ���ù۲���ģʽʵ�� */
#define LV_USE_OBSERVER 0

/** 1: ����ƴ�����뷨
 *  - Ҫ��lv_keyboard */
#define LV_USE_IME_PINYIN 0
#if LV_USE_IME_PINYIN
    /** 1: ʹ��Ĭ�ϴʿ�
     *  @ע�� �����ʹ��Ĭ�ϴʿ⣬��ȷ�������ôʿ��ʹ�� `lv_ime_pinyin` */
    #define LV_IME_PINYIN_USE_DEFAULT_DICT 1
    /** ���ÿ���ʾ������ѡ�������
     *  @ע�� ��Ҫ������Ļ��С���� */
    #define LV_IME_PINYIN_CAND_TEXT_NUM 6

    /** ʹ�� 9 �����루k9�� */
    #define LV_IME_PINYIN_USE_K9_MODE      1
    #if LV_IME_PINYIN_USE_K9_MODE == 1
        #define LV_IME_PINYIN_K9_CAND_TEXT_NUM 3
    #endif /*LV_IME_PINYIN_USE_K9_MODE*/
#endif

/** 1: �����ļ���Դ������
 *  - Ҫ��lv_table */
#define LV_USE_FILE_EXPLORER                     0
#if LV_USE_FILE_EXPLORER
    /** ·������󳤶� */
    #define LV_FILE_EXPLORER_PATH_MAX_LEN        (128)
    /** ���ٷ�������1: ʹ�ã�0: ��ʹ��
     *  - Ҫ��lv_list */
    #define LV_FILE_EXPLORER_QUICK_ACCESS        1
#endif

/** 1: ������������� */
#define LV_USE_FONT_MANAGER                     0
#if LV_USE_FONT_MANAGER

/** ���������������󳤶� */
#define LV_FONT_MANAGER_NAME_MAX_LEN            32

#endif

/** ����ģ�������豸��ʱ��ģ�����Ļ��ͼ�Ƚ� */
#define LV_USE_TEST 0
#if LV_USE_TEST

/** ���� `lv_test_screenshot_compare`
 * Ҫ�� libpng �ͼ� MB �Ķ��� RAM */
#define LV_USE_TEST_SCREENSHOT_COMPARE 0
#endif /*LV_USE_TEST*/

/** ��������ʱ���� XML UI */
#define LV_USE_XML    0

/*1: ������ɫ��������ʽ */
#define LV_USE_COLOR_FILTER     0
/*==================
 * �豸
 *==================*/

/** �� PC ��ʹ�� SDL �򿪴��ڲ��������ͼ��� */
#define LV_USE_SDL              0
#if LV_USE_SDL
    #define LV_SDL_INCLUDE_PATH     <SDL2/SDL.h>
    #define LV_SDL_RENDER_MODE      LV_DISPLAY_RENDER_MODE_DIRECT   /**< �Ƽ�ʹ�� LV_DISPLAY_RENDER_MODE_DIRECT �Ի��������� */
    #define LV_SDL_BUF_COUNT        1    /**< 1 �� 2 */
    #define LV_SDL_ACCELERATED      1    /**< 1: ʹ��Ӳ������ */
    #define LV_SDL_FULLSCREEN       0    /**< 1: Ĭ��ʹ����ȫ�� */
    #define LV_SDL_DIRECT_EXIT      1    /**< 1: ������ SDL ���ڹر�ʱ�˳�Ӧ�ó��� */
    #define LV_SDL_MOUSEWHEEL_MODE  LV_SDL_MOUSEWHEEL_MODE_ENCODER  /*LV_SDL_MOUSEWHEEL_MODE_ENCODER/CROWN*/
#endif

/** �� Linux ������ʹ�� X11 �򿪴��ڲ��������ͼ��� */
#define LV_USE_X11              0
#if LV_USE_X11
    #define LV_X11_DIRECT_EXIT         1  /**< ������ X11 ���ڹر�ʱ�˳�Ӧ�ó��� */
    #define LV_X11_DOUBLE_BUFFER       1  /**< ʹ��˫���������Ⱦ */
    /* ��ѡ��������Ⱦģʽ֮һ����ѡ LV_X11_RENDER_MODE_PARTIAL���� */
    #define LV_X11_RENDER_MODE_PARTIAL 1  /**< ������Ⱦģʽ����ѡ�� */
    #define LV_X11_RENDER_MODE_DIRECT  0  /**< ֱ����Ⱦģʽ */
    #define LV_X11_RENDER_MODE_FULL    0  /**< ��ȫ��Ⱦģʽ */
#endif

/** �� Linux �� BSD ������ʹ�� Wayland �򿪴��ڲ��������� */
#define LV_USE_WAYLAND          0
#if LV_USE_WAYLAND
    #define LV_WAYLAND_BUF_COUNT            1    /**< ���ڲ�����Ⱦģʽʹ�� 1�������壩��������ȫ��Ⱦģʽʹ�� 2��˫���壩 */
    #define LV_WAYLAND_USE_DMABUF           0    /**< Ϊ֡������ʹ�� DMA ��������Ҫ�� LV_DRAW_USE_G2D */
    #define LV_WAYLAND_RENDER_MODE          LV_DISPLAY_RENDER_MODE_PARTIAL   /**< DMABUF ֧�� LV_DISPLAY_RENDER_MODE_FULL �� LV_DISPLAY_RENDER_MODE_DIRECT */
                                                                             /**< ���� LV_WAYLAND_USE_DMABUF ʱ����֧�� LV_DISPLAY_RENDER_MODE_PARTIAL */
    #define LV_WAYLAND_WINDOW_DECORATIONS   0    /**< ���� Mutter/GNOME ����Ҫ���ƿͻ��˴���װ�Ρ�ʹ�� DMABUF ʱ��֧�� */
    #define LV_WAYLAND_WL_SHELL             0    /**< ʹ�ô�ͳ�� wl_shell Э�������Ĭ�ϵ� XDG shell */
#endif

/** /dev/fb ���������� */
#define LV_USE_LINUX_FBDEV      0
#if LV_USE_LINUX_FBDEV
    #define LV_LINUX_FBDEV_BSD           0
    #define LV_LINUX_FBDEV_RENDER_MODE   LV_DISPLAY_RENDER_MODE_PARTIAL
    #define LV_LINUX_FBDEV_BUFFER_COUNT  0
    #define LV_LINUX_FBDEV_BUFFER_SIZE   60
    #define LV_LINUX_FBDEV_MMAP          1
#endif

/** ʹ�� Nuttx �򿪴��ڲ����������� */
#define LV_USE_NUTTX    0

#if LV_USE_NUTTX
    #define LV_USE_NUTTX_INDEPENDENT_IMAGE_HEAP 0

    /** ΪĬ�ϻ��ƻ�����ʹ�ö�����ͼ��� */
    #define LV_NUTTX_DEFAULT_DRAW_BUF_USE_INDEPENDENT_IMAGE_HEAP    0

    #define LV_USE_NUTTX_LIBUV    0

    /** ʹ�� Nuttx �Զ����ʼ�� API �򿪴��ڲ����������� */
    #define LV_USE_NUTTX_CUSTOM_INIT    0

    /** /dev/lcd ���������� */
    #define LV_USE_NUTTX_LCD      0
    #if LV_USE_NUTTX_LCD
        #define LV_NUTTX_LCD_BUFFER_COUNT    0
        #define LV_NUTTX_LCD_BUFFER_SIZE     60
    #endif

    /** /dev/input ���������� */
    #define LV_USE_NUTTX_TOUCHSCREEN    0

    /* ����������С�����أ���<=0�����ù�꣩ */
    #define LV_NUTTX_TOUCHSCREEN_CURSOR_SIZE    0
#endif

/** /dev/dri/card ���������� */
#define LV_USE_LINUX_DRM        0

#if LV_USE_LINUX_DRM

    /* ʹ�� MESA GBM ������ͨ�� Linux DMA-BUF API ����ϵͳ��ʹ�ÿ�֮�乲���� DMA ��������
     * GBM ��ּ���ṩ������ƽ̨���ڴ����ϵͳ��֧����Ҫ GPU ��Ӧ�� - ��ѡ����Ҫ���� libgbm */
    #define LV_LINUX_DRM_GBM_BUFFERS 0
#endif

/** TFT_eSPI �ӿ� */
#define LV_USE_TFT_ESPI         0

/** evdev �����豸���������� */
#define LV_USE_EVDEV    0

/** libinput �����豸���������� */
#define LV_USE_LIBINPUT    0

#if LV_USE_LIBINPUT
    #define LV_LIBINPUT_BSD    0

    /** ��������֧�� */
    #define LV_LIBINPUT_XKB             0
    #if LV_LIBINPUT_XKB
        /** "setxkbmap -query" �ɰ����ҵ����̵���ȷֵ */
        #define LV_LIBINPUT_XKB_KEY_MAP { .rules = NULL, .model = "pc101", .layout = "us", .variant = NULL, .options = NULL }
    #endif
#endif

/* ͨ�� SPI/���ж˿����ӵ� LCD �豸�������� */
#define LV_USE_ST7735        0
#define LV_USE_ST7789        0
#define LV_USE_ST7796        0
#define LV_USE_ILI9341       0
#define LV_USE_FT81X         0

#if (LV_USE_ST7735 | LV_USE_ST7789 | LV_USE_ST7796 | LV_USE_ILI9341)
    #define LV_USE_GENERIC_MIPI 1
#else
    #define LV_USE_GENERIC_MIPI 0
#endif

/** Renesas GLCD ���������� */
#define LV_USE_RENESAS_GLCDC    0

/** ST LTDC ���������� */
#define LV_USE_ST_LTDC    0
#if LV_USE_ST_LTDC
    /* �����ڲ�����Ⱦ */
    #define LV_ST_LTDC_USE_DMA2D_FLUSH 0
#endif

/** LVGL Windows ��� */
#define LV_USE_WINDOWS    0

/** LVGL UEFI ��� */
#define LV_USE_UEFI 0
#if LV_USE_UEFI
    #define LV_USE_UEFI_INCLUDE "myefi.h"   /**< ����ʵ�ʿ�ܣ�EDK2, gnu-efi, ...����ͷ�ļ� */
    #define LV_UEFI_USE_MEMORY_SERVICES 0   /**< ʹ������������е��ڴ溯�� */
#endif

/** �� PC ��ʹ�� OpenGL �򿪴��ڲ��������ͼ��� */
#define LV_USE_OPENGLES   0
#if LV_USE_OPENGLES
    #define LV_USE_OPENGLES_DEBUG        1    /**< ���û���� opengles ���� */
#endif

/** QNX Screen ��ʾ�������������� */
#define LV_USE_QNX              0
#if LV_USE_QNX
    #define LV_QNX_BUF_COUNT        1    /**< 1 �� 2 */
#endif

/*=====================
* ����ѡ��
*======================*/

/** �������һ�𹹽���ʾ�� */
#define LV_BUILD_EXAMPLES 1

/** ������ʾ���� */
#define LV_BUILD_DEMOS 1

/*===================
 * ��ʾ����ʹ��
 ====================*/

#if LV_BUILD_DEMOS
    /** ��ʾһЩ������������Ҫ���� `LV_MEM_SIZE` */
    #define LV_USE_DEMO_WIDGETS 0
    
    /** ��ʾ�������ͼ��̵�ʹ�� */
    #define LV_USE_DEMO_KEYPAD_AND_ENCODER 0
    
    /** ��ϵͳ���л�׼���� */
    #define LV_USE_DEMO_BENCHMARK 0

    #if LV_USE_DEMO_BENCHMARK
        /** ʹ��λͼ���� 16 �ֽ��Ҳ���Ϊ Nx16 �ֽڵ����� */
        #define LV_DEMO_BENCHMARK_ALIGNED_FONTS 0
    #endif

    /** ��ÿ��ͼԪ������Ⱦ����
     *  - ������Ҫ 480x272 ����ʾ */
    #define LV_USE_DEMO_RENDER 0
    
    /** LVGL ��ѹ������ */
    #define LV_USE_DEMO_STRESS 0
    
    /** ���ֲ�������ʾ */
    #define LV_USE_DEMO_MUSIC 0
    #if LV_USE_DEMO_MUSIC
        #define LV_DEMO_MUSIC_SQUARE    0
        #define LV_DEMO_MUSIC_LANDSCAPE 0
        #define LV_DEMO_MUSIC_ROUND     0
        #define LV_DEMO_MUSIC_LARGE     0
        #define LV_DEMO_MUSIC_AUTO_PLAY 0
    #endif
    
    /** ʸ��ͼ����ʾ */
    #define LV_USE_DEMO_VECTOR_GRAPHIC  0
    
    /*---------------------------
     * ���� lvgl/lv_demos ����ʾ
      ---------------------------*/
    
    /** Flex ������ʾ */
    #define LV_USE_DEMO_FLEX_LAYOUT     0
    
    /** ���������ֻ��Ķ�������ʾ */
    #define LV_USE_DEMO_MULTILANG       0
    
    /** �����任��ʾ */
    #define LV_USE_DEMO_TRANSFORM       0
    
    /** ��ʾ�������� */
    #define LV_USE_DEMO_SCROLL          0
    
    /* ���� Lottie �����ĵ綯���г���ʾ����������� LV_USE_LOTTIE�� */
    #define LV_USE_DEMO_EBIKE           0
    #if LV_USE_DEMO_EBIKE
        #define LV_DEMO_EBIKE_PORTRAIT  0    /*0: ���� 480x270..480x320, 1: ���� 480x800..720x1280 */
    #endif
    
    /** �߷ֱ�����ʾ */
    #define LV_USE_DEMO_HIGH_RES        0
    
    /* �����ֱ���ʾ */
    #define LV_USE_DEMO_SMARTWATCH      0
#endif /* LV_BUILD_DEMOS */ 

/*--END OF LV_CONF_H--*/

#endif /*LV_CONF_H*/

#endif /*End of "Content enable"*/
