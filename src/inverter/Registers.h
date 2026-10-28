#ifndef SOFAR_REGISTERS_H
#define SOFAR_REGISTERS_H

// Sofar HYD (HYDV2 map) Modbus RTU registers and bus limits.
// Dependency-free so pure headers and native tests can use it.

#define MODBUS_SLAVE_ID   0x01
#define MODBUS_BAUD       9600
#define MODBUS_TIMEOUT_MS 400       // wait for first response byte
#define MODBUS_INTERBYTE_MS 20      // gap timeout for bytes within a frame
#define MODBUS_LISTEN_BUDGET_MS 900 // hard cap on one response read
#define MODBUS_MAX_FRAME  128
// id + fc + byteCount + data + crc16 must fit in one frame
#define MODBUS_MAX_READ_REGS ((MODBUS_MAX_FRAME - 5) / 2)
#define FC_READ_HOLDING   0x03
#define FC_WRITE_MULTI    0x10

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
#define REG_GRID_VOLTAGE   0x048D   // U16 / 10  → V  (L1)
#define REG_GRID_V_L2      0x0498   // U16 / 10  → V  (L2)
#define REG_GRID_V_L3      0x04A3   // U16 / 10  → V  (L3)
#define REG_LOAD_POWER     0x04AF   // S16 × 10  → W
// PV
#define REG_PV1_V          0x0584   // U16 / 10  → V
#define REG_PV1_A          0x0585   // U16 / 100 → A
#define REG_PV1_W          0x0586   // U16 × 10  → W
#define REG_PV2_V          0x0587   // U16 / 10  → V
#define REG_PV2_A          0x0588   // U16 / 100 → A
#define REG_PV2_W          0x0589   // U16 × 10  → W
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
// Serial number (8 regs = 16 ASCII chars)
#define REG_SN_START       0x0445
#define REG_SN_COUNT       8        // 0x0445-0x044C
// Control
#define REG_WORKING_MODE   0x1110
#define WORK_MODE_PASSIVE  3        // 0x1110 == 3 is Passive Mode
#define REG_PASSIVE_CTRL   0x1187   // Write 6 regs (3×32-bit): PPC, min, max
#define REG_PASSIVE_TIMEOUT 0x1184  // U16 seconds, 0 = disabled (factory default)
#define REG_PASSIVE_TMO_ACT 0x1185  // 0 = force standby, 1 = previous mode

static_assert(REG_SYS_COUNT     <= MODBUS_MAX_READ_REGS, "SYS block exceeds frame");
static_assert(REG_GRID_COUNT    <= MODBUS_MAX_READ_REGS, "GRID block exceeds frame");
static_assert(REG_BATT_COUNT    <= MODBUS_MAX_READ_REGS, "BATT block exceeds frame");
static_assert(REG_ENERGY_COUNT  <= MODBUS_MAX_READ_REGS, "ENERGY block exceeds frame");
static_assert(REG_LOAD_POWER < REG_GRID_START + REG_GRID_COUNT, "load power outside GRID block");
static_assert(REG_HS_TEMP    < REG_SYS_START  + REG_SYS_COUNT,  "heatsink temp outside SYS block");
static_assert(REG_BATT2_CYC  < REG_BATT_START + REG_BATT_COUNT, "batt2 cycles outside BATT block");
static_assert(REG_DIS_TOTAL + 1 < REG_ENERGY_START + REG_ENERGY_COUNT, "energy counter outside block");

#endif // SOFAR_REGISTERS_H
