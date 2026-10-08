/**
 * @file    w5500_demo.h
 * @brief   W5500 TCP 回显演示接口
 */
#ifndef __W5500_DEMO_H__
#define __W5500_DEMO_H__

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 启动 W5500 演示程序（内部死循环，不返回） */
void W5500_Demo_Run(void);

#ifdef __cplusplus
}
#endif

#endif /* __W5500_DEMO_H__ */
