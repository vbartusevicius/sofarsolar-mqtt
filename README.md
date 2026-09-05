# SofarSolar MQTT

A smart home interface for Sofar HYD-xx00-KTL (HYDV2) solar and battery inverters.

Forked from [Sofar2mqtt](https://github.com/IgorYbema/Sofar2mqtt) and rewritten as a modular, maintainable firmware targeting the **HYD 20 KTL** (HYDV2 register map). Runs on an ESP8266 with a TFT touchscreen and RS485 transceiver (e.g. the [Tindie module](https://www.tindie.com/products/thehognl/esp12-f-with-rs485-modbus-and-optional-touch-tft/)).

![Dashboard](docs/dashboard.png)

## Features

- **Modbus RTU** polling of ~90 HYDV2 registers (system, grid, PV×2, battery×2, energy counters)
- **MQTT** state publishing as a single JSON payload with configurable interval
- **Home Assistant** auto-discovery for ~45 sensors + a battery-saver switch
- **Battery Saver** mode — charges from excess solar, prevents grid discharge
- **ILI9341 TFT** with tabbed UI: live power-flow diagram (PV ⇄ Home ⇄ Grid/Battery, arrows show direction, boxes show values) + a system tab (version, IP, RSSI, heap, log tail), with a self-calibrating touch layer
- **Web dashboard** at `http://<device-ip>/` with live data, settings, and battery-saver control
- **WiFiManager** captive portal for first-time WiFi + MQTT setup
- **ArduinoOTA** for over-the-air firmware updates
- **Web firmware upload** — drop a `firmware.bin` in the web UI to flash over WiFi (no TLS, no cloud dependency)
- **TaskManagerIO** cooperative task scheduling (no blocking delays in the main loop)
- **ArduinoJson v7** for all JSON serialisation

## MQTT Topics

Subscribe to `<deviceName>/state` for a JSON payload containing:

### System
`run_state`, `inverter_temp`, `heatsink_temp`

### Grid
`grid_freq`, `inverter_power`, `grid_power`, `grid_voltage`, `load_power`

### PV
`pv1_voltage`, `pv1_current`, `pv1_power`, `pv2_voltage`, `pv2_current`, `pv2_power`, `pv_total`

### Battery 1
`batt_voltage`, `batt_current`, `batt_power`, `batt_temp`, `batt_soc`, `batt_soh`, `batt_cycles`

### Battery 2
`batt2_voltage`, `batt2_current`, `batt2_power`, `batt2_temp`, `batt2_soc`, `batt2_soh`, `batt2_cycles`

### Battery Totals
`batt_total_power`, `batt_avg_soc`, `batt_avg_soh`

### Energy (kWh)
`today_gen`, `total_gen`, `today_use`, `total_use`, `today_imp`, `total_imp`, `today_exp`, `total_exp`, `today_chg`, `total_chg`, `today_dis`, `total_dis`

### Status
`working_mode`, `battery_save`, `battery_save_target`, `modbus_ok`, `mqtt_ok`, `wifi_ok`, `uptime`

### Commands

These topics require the inverter to be in **Passive Mode**. All commands disable battery saver first, except `/set/battery_save` itself.

- **`<deviceName>/set/battery_save`** — payload `on`, `true`, or `1` enables battery saver. Any other payload disables it.
- **`<deviceName>/set/charge`** — payload is watts. Positive values charge the battery (e.g. `3000`), negative values discharge it (e.g. `-3000`).
- **`<deviceName>/set/standby`** — payload is ignored. Sets inverter to standby (0 W output).
- **`<deviceName>/set/auto`** — payload is watts (e.g. `5000`). Returns the inverter to autonomous mode with a charge/discharge limit of ±N watts. If the value is ≤ 0 or missing, defaults to ±16384 W (effectively unlimited).

### Battery Saver

When enabled, the firmware reads grid power every 3 seconds and adjusts the battery charge target so that only excess solar is stored. The battery **never discharges to the grid** — the charge target is clamped to 0–20000 W (`BSAVE_MAX_POWER`).

### Inverter flash protection

The passive-mode register block lives in the inverter's non-volatile memory, which has a limited write endurance. Writes are therefore minimised aggressively (all constants in `Config.h`):

- Target changes smaller than `BSAVE_MIN_DELTA` (100 W) are **not written** (hysteresis).
- An unchanged command is **not re-sent at all** by default. Earlier versions re-sent every 45 s on the inherited assumption that "the inverter times out passive mode after ~60 s". That is wrong for the HYD: the passive timeout is register `0x1184`, its factory default is `0` (disabled), and the shortest selectable value is 300 s — there is no 60 s setting. The firmware now reads `0x1184` at boot and hourly: `0` means no keep-alive writes ever, and any other value re-sends at half the timeout. The old behaviour was ~1,900 pointless writes/day.
- When the target stays at 0 W for `BSAVE_IDLE_LAPSE_MS` (10 min — e.g. overnight), writes stop completely and passive mode is allowed to lapse until solar surplus returns.
- All command paths (battery saver, MQTT, web UI) funnel through the same write cache, so redundant mode changes never produce duplicate register writes.

This reduces writes from ~28,800/day (older firmware) to a few dozen on a typical day and **zero at night**.

Why the caution: no public source states whether `0x1187` is EEPROM-backed or volatile, and Sofar does not document it. The solax-modbus project warns that *"most of the writeable parameters are written to EEPROM… typically 100000 write cycles"*, and both it and evcc write `0x1187` **only on change** — no integration re-asserts it periodically. Writing on change only is therefore both the safe choice and the ecosystem consensus.

### Tuning

The protection parameters are runtime-configurable in the web UI (**Battery Saver Tuning** panel) and persisted in EEPROM — no rebuild or reboot needed:

| Parameter | Default | Range | Meaning |
|---|---|---|---|
| Drift (W) | 100 | 20–2000 | Hysteresis: target changes smaller than this are not written |
| Max power (W) | 20000 | 0–20000 | Charge ceiling (matches HYD 20 KTL) |
| Idle lapse (min) | 10 | 1–60 | Sustained 0 W target before writes stop entirely |

Keep-alive is no longer a setting: it is derived from the inverter's own `0x1184`, and the detected value ("disabled — no keep-alive writes", or the timeout, its action, and the resulting interval) is shown read-only in the same panel. All remaining values are clamped server-side. The compiled defaults in `Config.h` are used for fresh devices.

The battery saver automatically tracks available solar surplus.


## Persisted State

EEPROM (the ESP8266's own emulated flash, unrelated to the inverter's) holds the device name and MQTT settings, the four battery-saver tuning values, the touch calibration, and the control state: current mode, charge power and auto limit. A reboot — yours, a firmware upload, or the supervisor's — resumes the mode that was running instead of silently falling back to `auto`.

Every write is dirty-checked: `EEPROM.commit()` rewrites a whole 4 kB sector, so re-issuing an unchanged mode (a retained MQTT command, a repeating automation) costs nothing.

## Long-Uptime Reliability

The device is meant to run unattended for months. The failure modes that matter at that timescale are not the ones that show up in a day of testing, so they are handled explicitly:

**`millis()` wraparound (every 49.7 days).** All elapsed-time checks use the unsigned `now - last >= interval` form, which is wrap-safe. A timestamp is never used as its own "unset" marker, because `millis()` legitimately returns 0 at boot and again at every wrap — timers that need an inactive state carry an explicit `bool` beside the timestamp (`BatterySaver::_zeroTiming`, `HealthState::wifiDown`). Wraparound cases are covered by native tests.

**Untrusted Modbus response lengths.** The byte count in an FC03 response arrives on the wire, and `readSensors()` reads into buffers as small as 2 bytes. A late reply to a previous, larger request — a well-formed frame with a valid CRC, just not the one that was asked for — used to be copied over the caller's stack. `modbusPayloadLen()` (`src/modbus/RespCheck.h`, unit-tested) validates the claimed length against both the received frame and the destination capacity, and `Inverter::readBlock` takes the capacity from the array type so a buffer cannot be resized without its bound following it.

**Bounded Modbus reads.** A response read has a hard wall-clock budget (`MODBUS_LISTEN_BUDGET_MS`). Without it, a bus that keeps delivering bytes addressed to another slave holds the read loop at its first byte forever, since every skipped byte restarts the first-byte timeout. With the link down (inverter asleep, RS485 unplugged) a single cheap probe read is attempted instead of all nine blocks, so a dead link costs one timeout per cycle rather than nine.

**Heap fragmentation.** On a chip with ~40 kB of heap, fragmentation kills long-running firmware sooner than leaks do. Nothing on a repeating path allocates: the LCD log view and `/log` walk the log ring buffer in place, `/json` streams the document straight to the socket, MQTT state is streamed with `beginPublish`/`endPublish`, and the SYS tab formats the IP from its octets rather than calling `toString()`. Heap, largest free block, fragmentation percentage and the low-water mark are visible in the web UI, the LCD SYS tab and MQTT.

**Watchdog.** The main loop is cooperative — no `delay()` over 20 ms outside serial gaps, and the Modbus wait loop yields. Blocking network calls are not attempted when they can only time out (MQTT does not try to connect while WiFi is down).

**Supervisor of last resort** (`src/util/Health.h`, unit-tested). Evaluated once a minute for the two states that cannot be recovered from in software:

| Condition | Action |
|---|---|
| WiFi down 2 min | force `WiFi.reconnect()`, repeated every 2 min |
| WiFi down 30 min | reboot |
| Free heap < 6 kB **or** largest block < 4 kB, 5 samples running | reboot |

A single low sample or a brief outage is deliberately ignored, and reconnecting resets the timers, so an intermittent access point never causes a reboot loop. `ESP.getResetReason()` is the first line in the log after every boot, so an unattended restart is always attributable.

**Inverter flash wear** is covered separately by the write cache described above — the point of that work is that months of uptime must not translate into months of writes.

## Testing

Unit tests for the safety-critical logic (Modbus CRC, inverter flash-write suppression, battery-saver hysteresis/lapse decisions, EEPROM dirty-guarding) run on the host via a `native` environment:

```bash
pio test -e native
```

or in Docker (what CI does):

```bash
docker build -f Dockerfile.test -t sofar-tests .
docker run --rm sofar-tests
```

GitHub Actions builds the firmware and runs the tests on every push/PR (`.github/workflows/ci.yml`).

## Building

This is a [PlatformIO](https://platformio.org/) project. All dependencies are managed automatically.

```bash
pio run            # compile (also regenerates .clangd for clangd IntelliSense)
pio run -t upload  # flash via USB
```

IDE note: `.clangd` is machine-specific and gitignored; `tools/gen_clangd.py` regenerates it automatically on every build (or run `python3 tools/gen_clangd.py` manually).

### Dependencies (auto-installed)

- Adafruit ILI9341, Adafruit GFX Library
- XPT2046_Touchscreen
- PubSubClient
- WiFiManager
- ArduinoJson v7
- TaskManagerIO

## Hardware

- **MCU:** ESP8266 (ESP-12F), 160 MHz
- **Display:** ILI9341 TFT (SPI) + XPT2046 touch
- **RS485:** Hardware Serial (TX=1, RX=3) to MAX485/MAX3485 transceiver
- **Pins:** TFT CS=D1, DC=D2, LED=D8, Touch CS=0, Touch IRQ=2

Connect RS485 A/B wires to the inverter's 485s port. Power the module from 5V USB.

## Touchscreen

Resistive panels have no absolute coordinate system: the XPT2046 reports a 12-bit ratio of a resistive divider whose usable range is specific to the individual panel, and the overlay's axis order and direction are independent of the TFT rotation. Both are therefore measured, never assumed.

**Gestures always work, calibrated or not:**

| Gesture | Action |
|---|---|
| Tap | Toggle battery saver (FLOW tab) |
| Hold 0.8 s | Switch tab |
| Hold 4 s | Start touch calibration |
| Tap while dimmed | Wake the backlight |

**Calibration** is started by holding the screen for 4 s, or from the web UI's *Touch Screen* panel. Tap the three crosshairs; the wizard averages the readings taken while each press is held, derives the axis order, direction and scale (`touchCalBuild()` in `src/display/TouchCal.h`), and stores 10 bytes in EEPROM. Bad input — saturated reads, a dead channel, or three taps in the same place — is rejected and the wizard restarts rather than saving a plausible-looking but wrong mapping. Once calibrated, the tab bar and the battery-saver button become directly tappable; the gestures remain as a fallback. Calibration state is shown in the web UI and on the LCD SYS tab, and every tap is logged to `/log` under `TCH`.

The mapping is pure, Arduino-free logic and is covered by native tests over all eight axis-order/inversion permutations.

## Releases & Firmware Update

Pushing to `main` triggers the **Release** workflow (`.github/workflows/release.yml`): it stamps `src/Version.h` with a `vYYYY.MM.DD.HHMM` (UTC) version, builds the firmware, and publishes a GitHub release with `firmware.bin`.

To update a device, download that `firmware.bin` and upload it in the web UI's **Firmware Update** panel (progress is shown; the device reboots when done). The running version is visible there, in the LCD SYS tab, in the MQTT state payload, and on the Home Assistant device page.

Automatic HTTPS self-updating was deliberately removed: a TLS download needs ~22 KB of contiguous heap plus ~6 KB of stack on a chip with ~40 KB total, and GitHub's asset CDN does not support TLS max-fragment-length negotiation — so it could not be made reliable. `pio run -t upload` (USB) and ArduinoOTA (`pio run -t upload --upload-port <ip>`) also remain available.

## Configuration

On first boot (or after factory reset), the device starts a **SofarBatterySaver** WiFi access point. Connect to it and configure:

- WiFi credentials
- Device name (used as MQTT topic prefix and mDNS hostname)
- MQTT host, port, username, password

Settings can also be changed via the web UI at `http://<device-ip>/`.

## Credits

Originally based on [Sofar2mqtt](https://github.com/cmcgerty/Sofar2MQTT) by Colin McGerty.
Version 2.0 rewrite by Adam Hill. Version 3.x by Igor Ybema (TFT, multi-inverter support).
CRC routines by Angelo Compagnucci and JP Mzometa.
HYDV2 rewrite and modularisation by Valentinas Bartusevičius.

![Photo](docs/photo.jpg)
