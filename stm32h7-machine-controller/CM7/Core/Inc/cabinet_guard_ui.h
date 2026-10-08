/**
 * @file    cabinet_guard_ui.h
 * @brief   CabinetGuard 四页 HMI（Overview / I·O / Parameters / System）
 */
#ifndef CABINET_GUARD_UI_H
#define CABINET_GUARD_UI_H

#ifdef __cplusplus
extern "C" {
#endif

/** 创建顶栏+底栏，并加载 Overview 首页 */
void cabinet_guard_ui_create(void);

/** 500ms 级刷新：传感器、告警、线圈同步 */
void cabinet_guard_ui_refresh(void);

#ifdef __cplusplus
}
#endif

#endif /* CABINET_GUARD_UI_H */
