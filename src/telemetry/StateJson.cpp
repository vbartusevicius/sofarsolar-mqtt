#include "telemetry/StateJson.h"
#include "inverter/Inverter.h"
#include "inverter/Derived.h"
#include "inverter/TelemetrySchema.h"
#include "control/BatterySaver.h"
#include "control/ModeController.h"
#include "Version.h"

void fillStateJson(JsonDocument& doc, const Inverter& inv,
                   const BatterySaver& bs, const ModeController& ctrl,
                   const LinkStatus& link)
{
    const InverterData& d = inv.data();
    char fb[16];

#define FILL_INT_FIELD(k, m, n, u, dc, sc)      doc[k] = d.m;
#define FILL_FLT_FIELD(k, m, dec, n, u, dc, sc) doc[k] = serialized(dtostrf(d.m, 0, dec, fb));
    SOFAR_TELEMETRY_INT_FIELDS(FILL_INT_FIELD)
    SOFAR_TELEMETRY_FLOAT_FIELDS(FILL_FLT_FIELD)
#undef FILL_INT_FIELD
#undef FILL_FLT_FIELD

    doc["run_state_name"]    = runStateName(d.runState);
    doc["working_mode_name"] = workingModeName(d.workingMode);
    doc["data_fresh"]        = d.fresh();
    // Mode & control
    doc["battery_save"]        = ctrl.saverActive();
    doc["battery_save_target"] = bs.targetPower();
    doc["bsave_strategy"]      = bs.strategyName();
    doc["mode"]          = ctrl.modeName();
    doc["charge_power"]  = ctrl.chargePower();
    doc["auto_limit"]    = ctrl.autoLimit();
    // Status
    doc["serial_number"]   = inv.serialNumber();
    doc["firmware"]        = FW_VERSION;
    doc["modbus_ok"]       = !inv.hasError();
    doc["passive_mode_ok"] = inv.isPassiveMode();
    doc["mqtt_ok"]         = link.mqttOk;
    doc["wifi_ok"]         = link.wifiOk;
    doc["uptime"]          = link.uptimeMs;
    doc["free_heap"]       = link.freeHeap;
    doc["heap_frag"]       = link.heapFrag;
    doc["max_free_block"]  = link.maxBlock;
    // Passive-mode write policy, read from the inverter (0x1184/0x1185)
    doc["passive_timeout_s"] = inv.passiveTimeoutS();
    doc["timeout_action"]    = inv.timeoutAction();
    doc["keepalive_s"]       = inv.keepaliveMs() / 1000;
}
