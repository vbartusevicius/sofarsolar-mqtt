#ifndef SOFAR_EE_LAYOUT_H
#define SOFAR_EE_LAYOUT_H

// ESP8266 EEPROM-emulation layout. Offsets are a persisted format: never move
// a field, only append. The static_asserts catch overlaps at compile time.

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
// Legacy Sofar2mqtt layout, written (not read) for settings compatibility
#define EE_INVERTER_MODEL 199 // 2 = HYDV2
#define EE_TFT_MODEL      200 // 1 = TFT present
#define EE_BSAVE_DELTA    204
#define EE_BSAVE_MAX      208
#define EE_KEEPALIVE_MS   212 // unused: keep-alive now comes from the inverter
#define EE_IDLE_LAPSE_MS  216
#define EE_TOUCH_CAL      220 // 10 bytes: valid, swap, 4x int16 raw reference
#define EE_TOUCH_CAL_LEN  10
#define EE_MODE           232 // 16 bytes: control mode name
#define EE_MODE_LEN        16
#define EE_CHARGE_POWER   248
#define EE_AUTO_LIMIT     252
#define EE_END            256

static_assert(EE_NAME  >= EE_MAGIC + 1,              "EE_NAME overlaps EE_MAGIC");
static_assert(EE_HOST  >= EE_NAME + EE_NAME_LEN,     "EE_HOST overlaps EE_NAME");
static_assert(EE_PORT  >= EE_HOST + EE_HOST_LEN,     "EE_PORT overlaps EE_HOST");
static_assert(EE_USER  >= EE_PORT + EE_PORT_LEN,     "EE_USER overlaps EE_PORT");
static_assert(EE_PASS  >= EE_USER + EE_USER_LEN,     "EE_PASS overlaps EE_USER");
static_assert(EE_INVERTER_MODEL >= EE_PASS + EE_PASS_LEN, "legacy bytes overlap EE_PASS");
static_assert(EE_BSAVE_DELTA   >= EE_TFT_MODEL + 1,     "tuning overlaps legacy bytes");
static_assert(EE_BSAVE_MAX     >= EE_BSAVE_DELTA + 4,   "EE_BSAVE_MAX overlaps");
static_assert(EE_KEEPALIVE_MS  >= EE_BSAVE_MAX + 4,     "EE_KEEPALIVE_MS overlaps");
static_assert(EE_IDLE_LAPSE_MS >= EE_KEEPALIVE_MS + 4,  "EE_IDLE_LAPSE_MS overlaps");
static_assert(EE_TOUCH_CAL     >= EE_IDLE_LAPSE_MS + 4, "EE_TOUCH_CAL overlaps");
static_assert(EE_MODE          >= EE_TOUCH_CAL + EE_TOUCH_CAL_LEN, "EE_MODE overlaps");
static_assert(EE_CHARGE_POWER  >= EE_MODE + EE_MODE_LEN, "EE_CHARGE_POWER overlaps");
static_assert(EE_AUTO_LIMIT    >= EE_CHARGE_POWER + 4,  "EE_AUTO_LIMIT overlaps");
static_assert(EE_END           >= EE_AUTO_LIMIT + 4,    "EE_END too small");
static_assert(EE_END <= EE_SIZE, "EEPROM layout exceeds EE_SIZE");

#endif // SOFAR_EE_LAYOUT_H
