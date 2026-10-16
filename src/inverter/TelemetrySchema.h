#ifndef SOFAR_TELEMETRY_SCHEMA_H
#define SOFAR_TELEMETRY_SCHEMA_H

// Single source of truth for InverterData telemetry fields shared by
// MqttManager::fillState() and Home Assistant Discovery (Discovery.cpp).
//
// Int fields:   X(jsonKey, member, haName, unit, devClass, stateClass)
// Float fields: X(jsonKey, member, decimals, haName, unit, devClass, stateClass)

#define SOFAR_TELEMETRY_INT_FIELDS(X) \
    X("run_state",        runState,       "Run State",            "",   nullptr,       "measurement") \
    X("inverter_temp",    inverterTemp,   "Inverter Temperature", "°C", "temperature", "measurement") \
    X("heatsink_temp",    heatsinkTemp,   "Heatsink Temperature", "°C", "temperature", "measurement") \
    X("inverter_power",   inverterPower,  "Inverter Power",       "W",  "power",       "measurement") \
    X("grid_power",       gridPower,      "Grid Power",           "W",  "power",       "measurement") \
    X("load_power",       loadPower,      "Load Power",           "W",  "power",       "measurement") \
    X("pv1_power",        pv1Power,       "PV1 Power",            "W",  "power",       "measurement") \
    X("pv2_power",        pv2Power,       "PV2 Power",            "W",  "power",       "measurement") \
    X("pv_total",         pvPower,        "Solar Power Total",    "W",  "power",       "measurement") \
    X("batt_power",       batteryPower,   "Battery Power",        "W",  "power",       "measurement") \
    X("batt_temp",        battTemp,       "Battery Temperature",  "°C", "temperature", "measurement") \
    X("batt_soc",         batterySOC,     "Battery SOC",          "%",  "battery",     "measurement") \
    X("batt_soh",         battSOH,        "Battery SOH",          "%",  nullptr,       "measurement") \
    X("batt_cycles",      battCycles,     "Battery Cycles",       "",   nullptr,       "measurement") \
    X("batt2_power",      batt2Power,     "Battery 2 Power",      "W",  "power",       "measurement") \
    X("batt2_temp",       batt2Temp,      "Battery 2 Temperature","°C", "temperature", "measurement") \
    X("batt2_soc",        batt2SOC,       "Battery 2 SOC",        "%",  "battery",     "measurement") \
    X("batt2_soh",        batt2SOH,       "Battery 2 SOH",        "%",  nullptr,       "measurement") \
    X("batt2_cycles",     batt2Cycles,    "Battery 2 Cycles",     "",   nullptr,       "measurement") \
    X("batt_total_power", battTotalPower, "Battery Total Power",  "W",  "power",       "measurement") \
    X("batt_avg_soc",     battAvgSOC,     "Battery Avg SOC",      "%",  "battery",     "measurement") \
    X("batt_avg_soh",     battAvgSOH,     "Battery Avg SOH",      "%",  nullptr,       "measurement") \
    X("working_mode",     workingMode,    "Working Mode",         "",   nullptr,       "measurement")

#define SOFAR_TELEMETRY_FLOAT_FIELDS(X) \
    X("grid_freq",     gridFreq,         2, "Grid Frequency",           "Hz",  "frequency", "measurement") \
    X("grid_voltage",  gridVoltage,      1, "Grid Voltage",             "V",   "voltage",   "measurement") \
    X("pv1_voltage",   pv1Voltage,       1, "PV1 Voltage",              "V",   "voltage",   "measurement") \
    X("pv1_current",   pv1Current,       2, "PV1 Current",              "A",   "current",   "measurement") \
    X("pv2_voltage",   pv2Voltage,       1, "PV2 Voltage",              "V",   "voltage",   "measurement") \
    X("pv2_current",   pv2Current,       2, "PV2 Current",              "A",   "current",   "measurement") \
    X("batt_voltage",  battVoltage,      1, "Battery Voltage",          "V",   "voltage",   "measurement") \
    X("batt_current",  battCurrent,      2, "Battery Current",          "A",   "current",   "measurement") \
    X("batt2_voltage", batt2Voltage,     1, "Battery 2 Voltage",        "V",   "voltage",   "measurement") \
    X("batt2_current", batt2Current,     2, "Battery 2 Current",        "A",   "current",   "measurement") \
    X("today_gen",     todayGeneration,  2, "PV Generation Today",      "kWh", "energy",    "total_increasing") \
    X("total_gen",     totalGeneration,  1, "PV Generation Total",      "kWh", "energy",    "total_increasing") \
    X("today_use",     todayConsumption, 2, "Consumption Today",        "kWh", "energy",    "total_increasing") \
    X("total_use",     totalConsumption, 1, "Consumption Total",        "kWh", "energy",    "total_increasing") \
    X("today_imp",     todayImport,      2, "Import Today",             "kWh", "energy",    "total_increasing") \
    X("total_imp",     totalImport,      1, "Import Total",             "kWh", "energy",    "total_increasing") \
    X("today_exp",     todayExport,      2, "Export Today",             "kWh", "energy",    "total_increasing") \
    X("total_exp",     totalExport,      1, "Export Total",             "kWh", "energy",    "total_increasing") \
    X("today_chg",     todayCharged,     2, "Battery Charged Today",    "kWh", "energy",    "total_increasing") \
    X("total_chg",     totalCharged,     1, "Battery Charged Total",    "kWh", "energy",    "total_increasing") \
    X("today_dis",     todayDischarged,  2, "Battery Discharged Today", "kWh", "energy",    "total_increasing") \
    X("total_dis",     totalDischarged,  1, "Battery Discharged Total", "kWh", "energy",    "total_increasing")

#endif // SOFAR_TELEMETRY_SCHEMA_H
