# SofarSolar MQTT

A smart home interface for Sofar HYD-xx00-KTL (HYDV2) solar and battery inverters.

Forked from [Sofar2mqtt](https://github.com/IgorYbema/Sofar2mqtt) and rewritten as a modular, maintainable firmware targeting the **HYD 20 KTL** (HYDV2 register map). Runs on an ESP8266 with a TFT touchscreen and RS485 transceiver (e.g. the [Tindie module](https://www.tindie.com/products/thehognl/esp12-f-with-rs485-modbus-and-optional-touch-tft/)).

## Features

- **Modbus RTU** polling of ~90 HYDV2 registers (system, grid, PV×2, battery×2, energy counters)
- **MQTT** state publishing as a single JSON payload with configurable interval
- **Home Assistant** auto-discovery for ~45 sensors + a battery-saver switch
- **Battery Saver** mode — charges from excess solar, prevents grid discharge
- **ILI9341 TFT** dashboard with XPT2046 touch toggle for battery saver
- **Web dashboard** at `http://<device-ip>/` with live data, settings, and battery-saver control
- **WiFiManager** captive portal for first-time WiFi + MQTT setup
- **ArduinoOTA** for over-the-air firmware updates
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

### Commands (Passive Mode)

| Topic | Payload | Description |
|---|---|---|
| `<deviceName>/set/battery_save` | `on` / `off` | Enable/disable battery saver |
| `<deviceName>/set/charge` | `0`–`20000` (watts) | Force charge at specified power |
| `<deviceName>/set/standby` | any | Set inverter to standby |
| `<deviceName>/set/auto` | `battery_save` or power window | Auto mode or battery saver |


## Building

This is a [PlatformIO](https://platformio.org/) project. All dependencies are managed automatically.

```bash
pio run            # compile
pio run -t upload  # flash via USB
```

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

