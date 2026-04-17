#ifndef SOFAR_CONFIG_H
#define SOFAR_CONFIG_H

#include <Arduino.h>

// ── Hardware Pins (ESP8266 TFT board from Tindie) ──────────────
#define PIN_TFT_CS    D1
#define PIN_TFT_DC    D2
#define PIN_TFT_LED   D8
#define PIN_TOUCH_CS  0
#define PIN_TOUCH_IRQ 2

// ── Modbus / RS-485 ────────────────────────────────────────────
// Hardware Serial (TX=1, RX=3) is wired to the RS-485 transceiver.
#define MODBUS_SLAVE_ID   0x01
#define MODBUS_BAUD       9600
#define MODBUS_TIMEOUT_MS 400       // per-byte wait
#define MODBUS_MAX_FRAME  128
#define FC_READ_HOLDING   0x03
#define FC_WRITE_MULTI    0x10

// ── Sofar HYDV2 Register Map ────────────────────────────────────
// Bulk-read block definitions
#define REG_SYS_START      0x0404
#define REG_SYS_COUNT      23       // 0x0404-0x041A
#define REG_GRID_START     0x0484
#define REG_GRID_COUNT     44       // 0x0484-0x04AF
#define REG_PV_START       0x0584
#define REG_PV_COUNT       6        // 0x0584-0x0589
#define REG_BATT_START     0x0604
#define REG_BATT_COUNT     14       // 0x0604-0x0611
#define REG_BATTTOT_START  0x0667
#define REG_BATTTOT_COUNT  3        // 0x0667-0x0669
#define REG_ENERGY_START   0x0684
#define REG_ENERGY_COUNT   24       // 0x0684-0x069B
// System
#define REG_RUNSTATE       0x0404
#define REG_INV_TEMP       0x0418   // S16 → °C
#define REG_HS_TEMP        0x041A   // S16 → °C
// Grid
#define REG_GRID_FREQ      0x0484   // U16 / 100 → Hz
#define REG_INV_POWER      0x0485   // S16 × 10  → W
#define REG_GRID_POWER     0x0488   // S16 × 10  → W  (+export / −import)
#define REG_GRID_VOLTAGE   0x048D   // U16 / 10  → V
#define REG_LOAD_POWER     0x04AF   // S16 × 10  → W
// PV
#define REG_PV1_V          0x0584   // U16 / 10  → V
#define REG_PV1_A          0x0585   // U16 / 100 → A
#define REG_PV1_W          0x0586   // U16 × 10  → W
#define REG_PV2_V          0x0587   // U16 / 10  → V
#define REG_PV2_A          0x0588   // U16 / 100 → A
#define REG_PV2_W          0x0589   // U16 × 10  → W
#define REG_PV_TOTAL       0x05C4   // U16 × 100 → W
// Battery 1
#define REG_BATT_V         0x0604   // U16 / 10  → V
#define REG_BATT_A         0x0605   // S16 / 100 → A
#define REG_BATT_W         0x0606   // S16 × 10  → W
#define REG_BATT_TEMP      0x0607   // S16 → °C
#define REG_BATT_SOC       0x0608   // U16 → %
#define REG_BATT_SOH       0x0609   // U16 → %
#define REG_BATT_CYC       0x060A   // U16
// Battery 2
#define REG_BATT2_V        0x060B   // U16 / 10  → V
#define REG_BATT2_A        0x060C   // S16 / 100 → A
#define REG_BATT2_W        0x060D   // S16 × 10  → W
#define REG_BATT2_TEMP     0x060E   // S16 → °C
#define REG_BATT2_SOC      0x060F   // U16 → %
#define REG_BATT2_SOH      0x0610   // U16 → %
#define REG_BATT2_CYC      0x0611   // U16
// Battery totals
#define REG_BATT_TOT_W     0x0667   // S16 × 100 → W
#define REG_BATT_AVG_SOC   0x0668   // U16 → %
#define REG_BATT_AVG_SOH   0x0669   // U16 → %
// Energy counters (U32 = 2 registers each)
#define REG_GEN_TODAY      0x0684   // U32 / 100 → kWh
#define REG_GEN_TOTAL      0x0686   // U32 / 10  → kWh
#define REG_USE_TODAY      0x0688   // U32 / 100 → kWh
#define REG_USE_TOTAL      0x068A   // U32 / 10  → kWh
#define REG_IMP_TODAY      0x068C   // U32 / 100 → kWh
#define REG_IMP_TOTAL      0x068E   // U32 / 10  → kWh
#define REG_EXP_TODAY      0x0690   // U32 / 100 → kWh
#define REG_EXP_TOTAL      0x0692   // U32 / 10  → kWh
#define REG_CHG_TODAY      0x0694   // U32 / 100 → kWh
#define REG_CHG_TOTAL      0x0696   // U32 / 10  → kWh
#define REG_DIS_TODAY      0x0698   // U32 / 100 → kWh
#define REG_DIS_TOTAL      0x069A   // U32 / 10  → kWh
// Control
#define REG_WORKING_MODE   0x1110
#define REG_PASSIVE_CTRL   0x1187   // Write 6 regs (3×32-bit): PPC, min, max

// ── Timing (milliseconds) ──────────────────────────────────────
#define INTERVAL_BSAVE     3000
#define INTERVAL_SENSORS   5000
#define INTERVAL_DISPLAY   1000
#define INTERVAL_MQTT_PUB  10000
#define INTERVAL_MQTT_RETRY 30000
#define SCREEN_DIM_MS      30000

// ── Battery Saver Limits ───────────────────────────────────────
#define BSAVE_MAX_POWER    20000    // W  (20 kW for HYD 20 KTL)

// ── EEPROM layout (kept compatible with original Sofar2mqtt) ───
#define EE_SIZE       512
#define EE_MAGIC      0       //  1 byte  '1' = configured
#define EE_NAME       1       // 64 bytes
#define EE_NAME_LEN   64
#define EE_HOST       65      // 64 bytes
#define EE_HOST_LEN   64
#define EE_PORT       129     //  6 bytes
#define EE_PORT_LEN   6
#define EE_USER       135     // 32 bytes
#define EE_USER_LEN   32
#define EE_PASS       167     // 32 bytes
#define EE_PASS_LEN   32

#endif // SOFAR_CONFIG_H
