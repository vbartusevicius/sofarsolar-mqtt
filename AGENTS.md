# AGENTS.md

Working notes for this repository. Read this before changing anything — several
of the constants here look arbitrary but encode findings that were expensive to
establish, and at least one inherited "fact" in the original code was wrong.

## Project

ESP8266 firmware for a **Sofar HYD 20 KTL (HYDV2 register map)** hybrid
inverter: Modbus RTU polling, MQTT + Home Assistant discovery, a web UI, and an
ILI9341 touch LCD. Forked from Sofar2mqtt and largely rewritten.

Hardware: ESP-12E/F @ 160 MHz, 4 MB flash. RS485 on the hardware UART at 9600
baud, Modbus slave `0x01`. ILI9341 TFT + XPT2046 touch on SPI
(TFT CS=D1, DC=D2, LED=D8, touch CS=GPIO0, touch IRQ=GPIO2).

## Build, test, verify

```bash
~/.platformio/penv/bin/pio run            # firmware (default env: esp8266)
~/.platformio/penv/bin/pio test -e native # 71 native gtest cases, must all pass
```

Both must pass before any change is considered done. CI
(`.github/workflows/ci.yml`) runs the firmware build and the native tests in
Docker (`Dockerfile.test`).

Budget check after building: currently ~44 % flash, ~50 % RAM. A change that
moves RAM by more than a few hundred bytes deserves an explanation.

Other tooling:
- `tools/gen_clangd.py` regenerates the machine-specific `.clangd` (gitignored;
  do not hand-maintain it). `test/.clangd` is separate so native tests use the
  host toolchain rather than the xtensa one.
- `tools/compiledb_hook.py` refreshes `compile_commands.json` after builds.

## Conventions

- **Edit source with real edits, not scripted rewrites.** The user reviews
  diffs; a whole-file rewrite is unreviewable. Never edit `.cpp`/`.h` by piping
  them through Python.
- Do not add or remove comments that are not part of the change.
- Keep decision logic **pure and testable**: anything with timing, arithmetic or
  policy goes in a dependency-free header under `src/` and gets native tests.
  Existing examples: `modbus/Crc16.h`, `modbus/RespCheck.h`,
  `inverter/PassiveWriteCache.h`, `control/SaverAlgorithm.h`, `display/TouchCal.h`,
  `util/Health.h`.
- Layering: `Modbus` (transport) → `Inverter` (registers/commands) →
  `BatterySaver` / `ModeController` (policy) → `MqttManager` / `SofarWebServer` /
  `Display` (presentation). `Display` knows nothing about EEPROM; it reports
  via callback and `main.cpp` persists.
- **Security is deliberately out of scope.** Web endpoints are unauthenticated
  and MQTT credentials are stored and served in plaintext. Do not "fix" this
  unless asked; do mention it if relevant.

## The inverter's flash is the point of this project

The original firmware wrote `REG_PASSIVE_CTRL` every 3 s — ~28,800 writes/day.
Everything below exists to avoid that.

### Passive control registers (verified against solax-modbus and evcc)

| Register | Meaning |
|---|---|
| `0x1110` | Energy storage mode (3 = Passive) |
| `0x1184` | Passive timeout, seconds. **Factory default 0 = disabled** |
| `0x1185` | Timeout action: 0 = force standby, 1 = return to previous mode |
| `0x1187` S32 | Desired grid power \| written as one 6-register block |
| `0x1189` S32 | Battery power min \| starting at `0x1187` |
| `0x118B` S32 | Battery power max \| |

### The 60-second myth — do not reintroduce it

`Config.h` used to say *"inverter times out passive mode after ~60 s"* and the
firmware re-sent an unchanged command every 45 s because of it. **This is false
for the HYD.** The passive timeout is register `0x1184`; its factory default is
`0` (disabled), and the shortest selectable value is **300 s** — there is no
60 s option. The claim was inherited from Sofar2mqtt's ME3000 lineage, which
sends a 9 s *heartbeat* frame (function `0x49`) on a completely different
register path (`0x0100–0x0103`, function `0x42`) — not a re-write of `0x1187`.

Consequence: the 45 s keep-alive was ~1,900 pointless writes/day.

The firmware now reads `0x1184` at boot and hourly and derives the policy:
`0` → never re-send an unchanged command; non-zero → re-send at half the
timeout. `keepaliveMs == 0` means "never" in both `passiveWriteDue()` and
`saverShouldSend()`, guarded explicitly — without the guard,
`now - lastAt >= 0` is always true and the meaning inverts to "always write".

### Is `0x1187` EEPROM-backed?

**Unknown, and no public source settles it.** Sofar does not document it. What
is known:
- solax-modbus warns: *"most of the writeable parameters are written to EEPROM
  of the inverter after each modification. EEPROM has a limited (typically
  100000) number of write cycles."*
- Both solax-modbus and evcc write `0x1187` **only on change** — evcc's
  `templates/definition/meter/sofarsolar-g3.yaml` uses a `switch` over battery
  modes. No integration re-asserts it periodically.
- Sofar2mqtt's years of 3-second writes are **not** evidence that this register
  is safe to hammer: it drives the ME3000 path, not `0x1187`.

Treat writes to `0x1187` as potentially wearing. Write on change only.

### Write-suppression policy

- `BSAVE_MIN_DELTA` (100 W) hysteresis; runtime-tunable 20–2000 W.
- Idle lapse: after `BSAVE_IDLE_LAPSE_MS` (10 min) at 0 W, writes stop entirely
  and passive mode is allowed to lapse (this is the overnight case).
- Zero crossings always write.
- Every command path (battery saver, MQTT, web, LCD) funnels through
  `Inverter::sendPassiveRange()` so nothing bypasses the cache.

## Two different flash memories

Do not conflate them:
- **Inverter NVM** — protected by the write cache above.
- **ESP8266 EEPROM emulation** — `EEPROM.commit()` rewrites a whole 4 kB
  sector, so `EEConfig::save()` compares every byte first and commits only on a
  real change. Tests assert that 20 identical saves cause zero commits.

EEPROM layout (`Config.h`, `EE_SIZE` 512): magic/name/MQTT settings at 0–198,
legacy compatibility bytes at 199–200, tuning at 204–219, touch calibration at
220–231, control state (mode/charge power/auto limit) at 232–255.
`EE_KEEPALIVE_MS` (212) is now unused — keep-alive comes from the inverter.

## Long-uptime rules (target: months unattended)

- **`millis()` wraps every 49.7 days.** Use `now - last >= interval` with
  unsigned types. **Never use a timestamp as its own "unset" marker** — 0 is a
  real `millis()` value at boot and at every wrap. Carry an explicit `bool`
  (`BatterySaver::_zeroTiming`, `HealthState::wifiDown`). Wraparound cases are
  unit-tested; add tests for any new timer.
- **Never trust a length that arrived on the wire.** `Modbus::listen()` copies
  into caller buffers as small as 2 bytes; `modbusPayloadLen()` validates the
  claimed count against both the received frame and the destination capacity.
  `Inverter::readBlock` has a template overload that takes the capacity from
  the array type, so a buffer cannot be resized without its bound following.
  The realistic attack is not noise but **desync**: a late reply to a previous,
  larger request is a valid frame with a valid CRC.
- **Bound every read loop in wall-clock time** (`MODBUS_LISTEN_BUDGET_MS`).
  Bytes addressed to another slave restart the first-byte timeout, so the loop
  can otherwise stall forever at `idx == 0`.
- **Nothing on a repeating path may allocate.** Heap fragmentation kills this
  chip long before leaks do. The LCD log view and `/log` walk the `AppLog` ring
  in place; `/json` and MQTT state stream straight to the socket; the SYS tab
  formats the IP from octets rather than `toString()`. `AppLog::text()` was
  deleted so it cannot be reintroduced.
- `delay(1)` and `yield()` both feed the software and hardware watchdogs. A
  blocking network call is a responsiveness problem, not a reset — do not
  "fix" a watchdog bug that is not there.
- With the Modbus link down, `readSensors()` probes with a single read instead
  of paying nine timeouts (the nightly case).
- `util/Health.h` is the supervisor of last resort, evaluated once a minute:
  WiFi down 2 min → `WiFi.reconnect()`; down 30 min → reboot; free heap < 6 kB
  or largest block < 4 kB for 5 consecutive samples → reboot. Single dips and
  brief outages are ignored on purpose — a supervisor that reboots a healthy
  device is worse than none. `ESP.getResetReason()` is logged first at boot.

## Touchscreen

Resistive panels have no absolute coordinate system, and the overlay's axis
order/direction is independent of the TFT rotation, so **nothing about the
mapping may be assumed**. Two attempts to guess it failed.

- `display/TouchCal.h` derives axis order, direction and scale from three
  measured points (A→B moves screen X only, A→C moves screen Y only). Tested
  over all eight wiring permutations. Bad input (saturated reads, a dead
  channel, three taps in one place) is rejected rather than saved.
- Calibration is stored in EEPROM and started by holding the screen 4 s or from
  the web UI.
- Gestures work with or without calibration: tap = battery saver, hold 0.8 s =
  switch tab, hold 4 s = calibrate.
- `XPT2046::touched()` gates on **pressure**, which dips during a sustained
  hold. Contact is debounced by `TS_RELEASE_MS` (250 ms); without it a long
  press restarts its own timer and never reaches the 4 s threshold.
- The backlight is PWM with `analogWriteRange` = **255** on this core. The
  original `analogWrite(PIN_TFT_LED, 32)` was 12 % brightness, which reads as
  "the display is dark".
- `Adafruit_GFX` has text wrap **on** by default: capping a log view by line
  count does not cap it by rendered rows. Disable wrap and cap rows.

## Firmware update

Local upload only: `POST /api/upload` (multipart) using the ESP8266 `Updater`
API, with `WiFiUDP::stopAll()` + `WiFiClient::stopAllExcept()` first to free a
contiguous block, and a 500 ms delayed restart so the response flushes.

**Do not reimplement GitHub HTTPS self-update.** It was tried and removed: TLS
needs ~22 KB of contiguous heap plus ~6 KB of stack on a chip with ~40 KB
total, and GitHub's asset CDN does not negotiate TLS max-fragment-length.
Removing it saved 119 KB of flash. `pio run -t upload` (USB) and ArduinoOTA
still work. CI still publishes `firmware.bin` for manual upload.

## MQTT

PubSubClient's `connected()` only inspects the socket, so behind a proxy
(Traefik/Docker) the session can be dead while the socket looks open. The
firmware publishes a sequence to `<device>/ping`, subscribes to the same topic,
and forces a reconnect after `MQTT_ECHO_MISS_TOLERANCE` missed round trips.
Home Assistant discovery is re-published on every connect, because retained
discovery does not survive a stateless broker restart.
