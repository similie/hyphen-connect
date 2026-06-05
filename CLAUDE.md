# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

**HyphenConnect** — an ESP32 library that abstracts cloud connectivity (WiFi *or* cellular) behind a single MQTT-backed API with remote function calls and variable registration. The repo is simultaneously:

1. **A publishable Arduino/PlatformIO library** — the real product. Library code lives in `src/` (`.cpp`) and `include/` (`.h`); `library.json` / `library.properties` describe the package.
2. **A PlatformIO firmware project** (`env:esp32dev`) used to test the library on a LilyGO T-PCIE board with a SIM7600 cellular modem.

This dual nature drives most of the non-obvious behavior below.

## Build / flash / monitor

All via PlatformIO CLI (`pio`). Two environments: `esp32dev` (firmware) and `native` (host unit tests).

```bash
pio run                 # compile firmware for env:esp32dev
pio run -t upload       # compile + flash over USB (upload_speed 921600)
pio run -t monitor      # serial monitor @ 115200, esp32_exception_decoder filter
pio device monitor      # same monitor standalone
pio run -t uploadfs     # flash the SPIFFS data/ partition (only if loading certs from FS)
pio run -t erase        # erase entire flash
pio run -t clean        # clean build artifacts
pio test -e native      # run host unit tests (no hardware); also runs in CI
```

## Native unit tests (`pio test -e native`)

Connection/reconnection and pure logic are tested on the host — no ESP32 needed, runs in CI (`.github/workflows/native-tests.yml`). Key design:

- The `[env:native]` block compiles **only** the natively-safe TUs (`build_src_filter`): `CommunicationRegistry.cpp`, `SubscriptionManager.cpp`, `ConnectionManager.cpp`. Hardware TUs (`Cellular.cpp`, `SecureMQTTProcessor.cpp`, `WiFiConnection.cpp`, `HyphenRunner.cpp`) are excluded — they're verified by the firmware build + on-device only.
- `test/native/shims/` provides a lightweight Arduino/ESP-IDF emulation (a `std::string`-backed `String`, controllable `millis()` via `test_clock.h`, no-op `Log`/`Ticker`/FreeRTOS). It's only on the native include path; the firmware build uses the real core. `-D ARDUINOJSON_ENABLE_ARDUINO_STRING=1` makes ArduinoJson serialize into the shim `String`.
- `test/native/mocks/` has `FakeConnection`/`FakeSecureClient`/`FakeProcessor` — scriptable doubles (drop/recover sequences via `std::deque<bool>`) that drive the managers under test.
- Production seams that make this possible: `ConnectionManager`'s injecting ctor (`vector<unique_ptr<Connection>>`) and `#ifndef HYPHEN_NATIVE_TEST` guards around its concrete-transport code; `SubscriptionManager::buildRegistryPayload()` + `#ifdef HYPHEN_NATIVE_TEST` test seams. Keep these intact when editing.

**Gotcha:** `src/certs/{root-ca,device-cert,private-key}.pem` are gitignored and have been silently renamed with a `" 2"` suffix by host file-sync before (breaking the embed step with "Source `src/certs/root-ca.pem' not found"). If the firmware build can't find a cert, check for `*\ 2.pem` variants in `src/certs/`.

Build configuration is **entirely** in `platformio.ini` `build_flags` — there is no separate config file. Every tunable in the code is an `#ifndef`-guarded macro, so changing behavior = editing `build_flags` (see "Configuration macros" below).

## The `src/main.cpp` gotcha

`src/main.cpp` is a **gitignored no-op stub** (`setup(){} loop(){}`). Building the project as-is produces empty firmware. To exercise the library, put your sketch in `src/main.cpp` — the working reference sketch is `examples/main.ino` (publish/subscribe + register a remote function and variable). Several files are gitignored and present only locally: `src/main.cpp`, `src/certs/`, `partitions.csv`, `data/`, `CMakeLists.txt`.

## Architecture

A strict layered design, each layer programming against an abstract base so WiFi and cellular are interchangeable. `HyphenConnect` (the facade) owns one of each and wires them in its constructor (`connection → processor → manager`):

```
HyphenConnect            public API; forwards everything to the managers below
  └ ConnectionManager    : Connection  — picks WiFi vs Cellular per ConnectionType
      ├ WiFiConnection   : Connection  — WiFiClientSecure
      └ Cellular         : Connection  — TinyGSM (SIM7600) + SSLClientESP32
  └ SecureMQTTProcessor  : Processor   — PubSubClient over the connection's SecureClient
  └ SubscriptionManager               — pub/sub, remote functions & variables, keep-alive
      └ CommunicationRegistry         — singleton topic→callback dispatch (wildcard match)
```

Key abstractions (each has a pure-virtual base; add a new transport by implementing these):
- **`Connection`** (`include/connections/Connection.h`) — transport lifecycle (`connect/maintain/init/keepAlive`) plus factory methods returning `Client&` / `SecureClient&`.
- **`SecureClient`** — a uniform TLS-client interface. Both transports provide a concrete wrapper (`WiFiSecureClient`, `CellularSecureClient`) that forwards to the platform TLS client; the cellular one guards every call with a recursive FreeRTOS mutex.
- **`Processor`** (`include/processors/Processor.h`) — the messaging layer; `SecureMQTTProcessor` is the only implementation.

`ConnectionType` (`WIFI_PREFERRED`, `CELLULAR_PREFERRED`, `WIFI_ONLY`, `CELLULAR_ONLY`, `NONE`) is passed to the `HyphenConnect` constructor and decides which `Connection`(s) `ConnectionManager` instantiates and how it fails over.

### Control-flow invariant

The reconnect logic hinges on a boolean convention: **`SubscriptionManager::loop()` returns `false` to signal the connection is broken**, which makes the caller tear down and rebuild the stack. `true` = healthy, keep looping. Preserve this when editing any `loop()`/`maintain()` in the chain.

**Reconnect backoff:** `HyphenRunner::initManager()` is the actual reconnect driver. Its retry loops space attempts with `hyphen::backoff::delayMs()` (`include/Backoff.h`) keyed on the existing `getConnectAttempts()` counter — exponential from `HYPHEN_RECONNECT_BACKOFF_BASE_MS` (500 ms) up to `HYPHEN_RECONNECT_BACKOFF_MAX_MS` (30 s), unlimited attempts but no storm. The counter resets on a successful connect (`resetConnectAttempts()`), so the backoff resets too. The schedule is pure and host-tested (`test/native/test_backoff`).

### Threading model (`HYPHEN_THREADED`)

- **Defined** (the default in `platformio.ini`): connection bring-up and the processing loop run on a dedicated FreeRTOS task pinned to **Core 0**, driven by the **`HyphenRunner`** singleton (`src/HyphenRunner.*`). `HyphenConnect::loop()` just calls `runner.loop()`. On a detected failure, `HyphenRunner::rebuildConnection()` tears down and re-inits the whole stack.
- **Undefined**: everything runs synchronously inside the Arduino `loop()`.

Both paths call the same `manager`/`connection`/`processor` methods — only *where* they run differs. Use `coreDelay()` (`include/managers/CoreDelay.h`) instead of `delay()` in task code: it yields in 100 ms chunks so the IDLE task and watchdog stay happy.

### Remote functions & variables

`hyphen.function(name, fn)` and `hyphen.variable(name, ptr)` register callbacks that the cloud invokes over MQTT. On (re)connect, `SubscriptionManager` publishes a registration message listing what the device supports, then listens on `Hy/Post/Function/<DeviceId>/#` and `Hy/Post/Variable/<DeviceId>/#`. Topic structure and result payloads are documented in `README.md` ("MQTT Default Topics Commands"). `MQTT_TOPIC_BASE` (default `Hy/`) prefixes every topic.

### TLS / certificates (mutual TLS)

Two ways to supply the client cert, CA, and private key:
1. **Embedded at build time** (default): `platformio.ini` `board_build.embed_txtfiles` embeds `src/certs/{root-ca,device-cert,private-key}.pem`. The linker exposes them as `_binary_src_certs_*_pem_start/end` symbols, consumed in `SecureMQTTProcessor.h`. **To rotate certs, replace those three PEM files** (gitignored).
2. **From SPIFFS** at runtime via `FileManager`, using paths from `MQTT_*_CERTIFICATE_NAME` macros — requires `pio run -t uploadfs`.

### PSRAM for TLS

`BOARD_HAS_PSRAM` + `CONFIG_SPIRAM_USE_CAPS_ALLOC` route large mbedTLS allocations to external PSRAM. `src/esp_mbedtls_psram_alloc.cpp` provides `esp_mbedtls_psram_calloc` backing this — needed because TLS handshake buffers would otherwise exhaust internal RAM.

## Configuration macros

Defaults are defined with `#ifndef` guards in the relevant headers; override in `platformio.ini` `build_flags`. The full annotated list is in `README.md` ("Available Macros"). The ones that change structural behavior:

- `CONNECTION_TYPE` / constructor arg — which transport(s) are used.
- `HYPHEN_THREADED` — run on a Core-0 task vs. inline (see above).
- `TINY_GSM_MODEM_SIM7600` + `CELLULAR_*` pins / `CELLULAR_APN` / `GSM_SIM_PIN` / `NETWORK_MODE` — cellular modem wiring & radio mode (LilyGO T-PCIE pinout).
- `MQTT_IOT_ENDPOINT` / `MQTT_IOT_PORT` / `DEVICE_PUBLIC_ID` — broker + device identity (current default endpoint is AWS IoT, port 8883, mTLS).
- `DEFAULT_WIFI_SSID` / `DEFAULT_WIFI_PASS` — seed WiFi credentials (escape spaces, e.g. `\"My\ SSID\"`).
- Opt-in flags: `INSECURE_MQTT` (plain client, no TLS), `DUMP_AT_COMMANDS` (StreamDebugger AT trace), `HYPHEN_WIFI_AP_ENABLE` (SoftAP via `staMode()`), `NO_CELLULAR_TEST_INTERVAL_MAINTAIN` (skip periodic cellular reachability probe).

## Conventions

- Logging goes through **ArduinoLog** (`Log.noticeln`, `Log.infoln`, etc.), gated by the level passed to `hyphen.setup(LOG_LEVEL_*)`. Don't use raw `Serial.print`.
- Header include dirs are split: public-ish headers in `include/`, but `src/` also holds `HyphenConnect.h`/`HyphenRunner.h`. The aggregator headers `Connections.h`, `Managers.h`, `Processors.h` just pull in the sub-headers.
- Directory casing differs between `include/` (lowercase `connections/`, `managers/`, `processors/`) and `src/` (capitalized `Connections/`, `Managers/`, `Processors/`) — match the existing case when adding files.
