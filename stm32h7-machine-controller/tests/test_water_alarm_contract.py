from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def test_water_state_is_exposed_by_app_data_and_ipc():
    app_data_h = read("BSP/bsp_app_data.h")
    ipc_cm4 = read("CM4/APP/IPC/app_ipc_cm4.c")
    ipc_cm7 = read("CM7/APP/IPC/app_ipc_cm7.c")

    assert "BSP_AppData_GetWet" in app_data_h
    assert "BSP_AppData_SetWet" in app_data_h
    assert "APP_IPC_ADC_WET_BIT" in ipc_cm7
    assert "BSP_AppData_SetWet" in ipc_cm7
    assert "msg.counter |= (1UL << APP_IPC_ADC_WET_BIT)" in ipc_cm4


def test_water_state_has_a_modbus_register_and_qt_reads_it():
    modbus_h = read("BSP/Modbus/bsp_modbus_app.h")
    dashboard = read("../../OTA (4)/OTA/ui/DashboardPage.cpp")

    assert "MODBUS_REG_WET" in modbus_h
    assert "readHoldingRegisters(kSlaveId, 0, 4" in dashboard
    assert "regs.at(3)" in dashboard
