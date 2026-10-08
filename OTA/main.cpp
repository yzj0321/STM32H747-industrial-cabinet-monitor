/**
 * @file    main.cpp
 * @brief   OTA 上位机 Qt 应用程序入口
 */

#include "mainwindow.h"

#include <QApplication>

/**
 * @brief  程序入口，初始化 Qt 应用并显示主窗口
 * @param  argc 命令行参数个数
 * @param  argv 命令行参数数组
 * @retval 应用程序退出码
 */
int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("OTA_Host"));
    QApplication::setApplicationVersion(QStringLiteral("2.1.0"));
    QApplication::setOrganizationName(QStringLiteral("STM32H7"));

    MainWindow window;
    window.show();
    return app.exec();
}
