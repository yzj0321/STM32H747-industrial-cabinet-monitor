QT += widgets serialport network

CONFIG += c++17

TARGET = OTA_Host
TEMPLATE = app

INCLUDEPATH += \
    protocol \
    transport \
    engine \
    ui \
    modbus

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    protocol/CrcUtils.cpp \
    protocol/OtaProtocol.cpp \
    transport/SerialManager.cpp \
    transport/NetworkManager.cpp \
    transport/LogClient.cpp \
    engine/UpdateEngine.cpp \
    modbus/ModbusRtuClient.cpp \
    ui/GaugeWidget.cpp \
    ui/AlarmCard.cpp \
    ui/DashboardPage.cpp \
    ui/LogPage.cpp

HEADERS += \
    mainwindow.h \
    protocol/CrcUtils.h \
    protocol/OtaProtocol.h \
    transport/OtaTransport.h \
    transport/SerialManager.h \
    transport/NetworkManager.h \
    transport/LogClient.h \
    engine/UpdateEngine.h \
    modbus/ModbusRtuClient.h \
    ui/GaugeWidget.h \
    ui/AlarmCard.h \
    ui/DashboardPage.h \
    ui/LogPage.h

# UI built in code; legacy form kept empty
# FORMS += mainwindow.ui

qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
