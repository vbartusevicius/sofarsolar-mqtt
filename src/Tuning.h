#ifndef SOFAR_TUNING_H
#define SOFAR_TUNING_H

// Scheduler intervals and control defaults. Dependency-free.

#define INTERVAL_POLL_STEP 1000     // 1 Modbus block + 1 control tick per run
#define INTERVAL_DISPLAY   1000
#define INTERVAL_MQTT_PUB  10000
#define INTERVAL_MQTT_RETRY 30000
#define MQTT_ECHO_PING_MS       60000UL  // this many missed broker echoes → force reconnect
#define MQTT_ECHO_MISS_TOLERANCE 3
#define SCREEN_DIM_MS      30000

#define BSAVE_MAX_POWER    20000    // W  (20 kW for HYD 20 KTL)
#define BSAVE_MIN_DELTA    100      // W  hysteresis: ignore smaller target changes
#define BSAVE_IDLE_LAPSE_MS 600000UL // stop writes after 10 min at 0 W (night)
#define AUTO_LIMIT_DEFAULT 16384    // W  symmetric passive window for "auto"

// Keep-alive is derived from the inverter's own passive timeout (0x1184), not
// assumed: the factory default is 0 = disabled, and the shortest selectable
// value is 300 s. 0 here means "never re-send an unchanged command".
#define PASSIVE_KEEPALIVE_MS 0UL

#endif // SOFAR_TUNING_H
