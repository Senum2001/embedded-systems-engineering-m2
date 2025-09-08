# EcoWatt Device — Milestone 2 (Team Pebble)

Short description: Modern C++17 implementation that integrates with the Inverter SIM using Modbus RTU frames encapsulated in HTTP/JSON. It polls a minimum set of registers (voltage, current), supports a single control point (export power %), validates CRC, retries on HTTP errors, and stores samples in memory and SQLite with structured logging.

## What’s implemented in this codebase

### 1) Protocol in use (Modbus over HTTP/JSON)
- Transport: Modbus RTU frames sent as JSON to the Inverter SIM.
- Endpoints (relative to base URL)
  - POST /api/inverter/read
  - POST /api/inverter/write
- Payload (both endpoints)
  - { "frame": "<HEX_STRING>" }
- Required headers
  - Content-Type: application/json
  - Accept: */*
  - Authorization: <API_KEY>
- Function codes supported
  - 0x03 Read Holding Registers
  - 0x06 Write Single Register
- CRC
  - Standard Modbus RTU CRC-16 (poly 0xA001, init 0xFFFF), LSB first in frame; all frames are validated in `ModbusFrame`.
- Error handling
  - Parses Modbus exception responses (function | 0x80) and maps codes 0x01–0x0B to messages.
  - HTTP transport errors (timeouts, non-2xx, bad JSON) are retried.

Code pointers
- Protocol adapter: `cpp/include/protocol_adapter.hpp`, `cpp/src/protocol_adapter.cpp`
- Modbus frames + CRC: `cpp/include/modbus_frame.hpp`, `cpp/src/modbus_frame.cpp`
- HTTP transport (cpprestsdk): `cpp/include/http_client.hpp`, `cpp/src/http_client.cpp`
- Exceptions and types: `cpp/include/exceptions.hpp`, `cpp/include/types.hpp`

### 2) Selected registers (read/write)
We meet the “minimum includes voltage and current” requirement by polling registers 0 and 1 each cycle. Register 8 is the only control point written in M2, and is read back for verification.

| Addr | Name                       | Unit | Gain | Access     | Usage                                |
|-----:|----------------------------|------|-----:|------------|--------------------------------------|
| 0    | Vac1_L1_Phase_voltage      | V    | 10.0 | Read       | Periodic read (minimum set)          |
| 1    | Iac1_L1_Phase_current      | A    | 10.0 | Read       | Periodic read (minimum set)          |
| 8    | Export_power_percentage    | %    | 1.0  | Read/Write | Control write; read-back to verify   |

Scaling in code: scaled_value = raw_value / gain

Register metadata lives in `cpp/config.json` under `registers` and is consumed by `ConfigManager`.

### 3) Runtime configuration
- Modbus (logical)
  - Slave address: 17 (0x11)
  - Timeout: 5000 ms
  - Max retries: 3
  - Retry delay: 1000 ms
  - Functions: 0x03 read holding, 0x06 write single
- Acquisition
  - Polling interval: 5000 ms (every 5 s)
  - Minimum registers: [0, 1]
  - Background polling: enabled
- API
  - Base URL: from .env → INVERTER_API_BASE_URL (defaults to http://20.15.114.131:8080 in `types.hpp`)
  - API Key: from .env → INVERTER_API_KEY
  - Endpoints: /api/inverter/read, /api/inverter/write
- Storage
  - Memory retention per register: 1000 samples
  - Persistent SQLite: enabled
  - Cleanup scheduled daily; retention: 30 days
- Logging
  - Console: INFO; File: DEBUG; file ecoWatt_milestone2.log

Files
- Config manager: `cpp/include/config_manager.hpp`, `cpp/src/config_manager.cpp`
- Defaults and shapes: `cpp/config.json`, `cpp/include/types.hpp`

### 4) Time intervals and behaviors
- Polling interval: every 5 seconds
- HTTP timeout: 5 seconds; up to 3 attempts with 1-second delay between attempts

### 5) Core test scenarios (what we verify)
Functional
1. Single register read (addr 0): returns 1 value; scaled = raw/10.0; value within a valid range.
2. Multi-register read (start 0, count 2..5): count matches; CRC validated; no error flag.
3. Write export power (addr 8, value 75): HTTP 200; response echoes address/value; read-back equals 75; restore original.

Protocol/Frame
4. CRC failure handling: corrupted response → ModbusException raised; no sample stored.
5. Illegal data address: read undefined register → error frame (func|0x80), mapped message.
6. Invalid register count: readRegisters(0, 126) → argument validation error.

Transport/Resilience
7. Timeout + retry: transient HTTP failure → up to 3 attempts with 1 s delay; success updates stats.
8. JSON error handling: empty or malformed frame field → request considered failed; retried or surfaced as exception.

Integration/End-to-End
9. Communication test (ProtocolAdapter::testCommunication): read [0..1], read 8, write 8=50, restore — returns true.
10. Acquisition loop smoke: start scheduler; after ~20 s, verify samples present for 0 and 1; success rate > 0.

Test sources
- `tests/test_protocol_adapter.cpp` (read/write, retries, errors)
- `tests/test_modbus_frame.cpp` (CRC, framing, parsing)
- `tests/test_api_integration.cpp` (HTTP path + JSON contract)
- `tests/test_acquisition_scheduler.cpp` (poll loop + scaling)
- `tests/test_error_scenarios.cpp`, `tests/test_data_storage.cpp`

## Build and run (Windows / PowerShell)

Prerequisites
- Visual Studio 2019/2022 with Desktop development with C++
- CMake ≥ 3.16
- vcpkg (recommended) and the following ports installed for x64-windows:
  - cpprestsdk, nlohmann-json, spdlog, sqlite3

Configure and build with CMake (from repo root)
```powershell
Set-Location cpp
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE="C:/vcpkg/scripts/buildsystems/vcpkg.cmake" -A x64
cmake --build build --config Release --parallel
```

Prepare runtime configuration (required at first run)
```powershell
# Create cpp/.env with your credentials and options
@'
INVERTER_API_KEY=REPLACE_WITH_YOUR_KEY
INVERTER_API_BASE_URL=http://20.15.114.131:8080
DATABASE_PATH=ecoWatt_milestone2.db
LOG_LEVEL=INFO
LOG_FILE=ecoWatt_milestone2.log
DEFAULT_SLAVE_ADDRESS=17
MAX_RETRIES=3
REQUEST_TIMEOUT_MS=5000
RETRY_DELAY_MS=1000
'@ | Out-File -FilePath .\.env -Encoding utf8 -Force
```

Run (post-build copies `config.json` and `.env` next to the EXE)
```powershell
Set-Location build/Release
./EcoWattDevice.exe --config "config.json" --env ".env"
```

Notes
- If you configure inside `cpp`, use `-S .` (not `-S cpp`).
- The CMake project links: cpprestsdk, nlohmann_json, spdlog, sqlite3.

## Key components map

- Entry point: `cpp/src/main.cpp` — wires config, logging, comms test, and starts acquisition (demo mode available).
- Protocol adapter: `cpp/include/protocol_adapter.hpp`, `cpp/src/protocol_adapter.cpp` — 0x03/0x06 frames, CRC validation, retries, JSON payloads.
- Modbus frame/CRC: `cpp/include/modbus_frame.hpp`, `cpp/src/modbus_frame.cpp` — frame creation/parsing, CRC-16 Modbus (LSB first).
- HTTP client: `cpp/include/http_client.hpp`, `cpp/src/http_client.cpp` — cpprestsdk-based POST/GET, timeouts, headers.
- Acquisition scheduler: `cpp/include/acquisition_scheduler.hpp`, `cpp/src/acquisition_scheduler.cpp` — background polling, scaling (raw/gain), sample callbacks.
- Storage: `cpp/include/data_storage.hpp`, `cpp/src/data_storage.cpp` — memory ring buffers + SQLite persistence; daily cleanup and retention.
- Config: `cpp/include/config_manager.hpp`, `cpp/src/config_manager.cpp`, `cpp/config.json`, `.env` — precedence: .env overrides JSON → code defaults.
- Logging: `cpp/include/logger.hpp`, `cpp/src/logger.cpp` — console INFO, rotating file DEBUG; file `ecoWatt_milestone2.log`.

## Inverter SIM API contract 

- Base URL: from `.env` INVERTER_API_BASE_URL (defaults to http://20.15.114.131:8080)
- Read: POST /api/inverter/read, body { "frame": "<HEX>" }, headers described above; returns { "frame": "<HEX>" }
- Write: POST /api/inverter/write, body { "frame": "<HEX>" }, returns echo frame on success or exception frame on error
