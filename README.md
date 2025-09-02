# EcoWatt Device — Milestone 2: Inverter SIM Integration and Basic Acquisition

A C++17 implementation that integrates the EcoWatt Device with the Inverter SIM over an HTTP API, implements a Modbus RTU protocol adapter (framing, CRC, parsing, retries), and a basic acquisition scheduler that polls key registers and stores samples in memory and SQLite.

This README explains how to build, configure, run, and validate the solution on Windows. It also documents key design points and troubleshooting tips aligned with the provided API documentation.

## Objectives and Scope

- Protocol adapter implementation
  - Modbus RTU request formatting and response parsing (0x03, 0x06)
  - CRC-16 (Modbus) computation (LSB first) and frame validation
  - Timeout detection, error/exception frames parsing, logging, and simple retry logic
  - HTTP transport to the Inverter SIM Read/Write endpoints using a bearer API key
- Basic data acquisition
  - Poll at least the minimal set: voltage (reg 0) and current (reg 1) at a configurable rate
  - Store raw and scaled samples in a modular storage (memory + SQLite)
  - Perform at least one write (export power percentage at reg 8)

## Repository layout

```
milestone2/
├─ README.md                ← This file
└─ cpp/                     ← C++ source, CMake project, build scripts
   ├─ CMakeLists.txt        ← Project build config
   ├─ include/              ← Headers (protocol adapter, scheduler, storage, etc.)
   ├─ src/                  ← Implementations
   ├─ config.json           ← App configuration (registers, polling, logging)
   ├─ .env                  ← Environment variables (API key, base URL) — user provided
   ├─ build.ps1 / build.bat ← Windows build helpers (vcpkg + CMake)
   └─ build/                ← CMake build output (after configuration)
```

Key runtime artifacts are copied next to the executable by the post-build step:
- EcoWattDevice.exe
- config.json (copied from cpp/config.json)
- .env (copied from cpp/.env)
- ecoWatt_milestone2.db (created on first run if enabled)
- ecoWatt_milestone2.log

## Architecture overview

```
┌──────────────┐   ┌────────────────┐   ┌───────────────────┐
│ EcoWatt      │   │ Acquisition     │   │ Protocol Adapter  │
│ Device       │◄──│ Scheduler       │◄──│ (Modbus over HTTP)│
└─────┬────────┘   └─────┬──────────┘   └──────────┬────────┘
      │                  │                         │
      ▼                  ▼                         ▼
┌──────────────┐   ┌──────────────┐         ┌───────────────┐
│ Data Storage │   │ Config       │         │ HTTP Client   │
│ (Mem+SQLite) │   │ Manager      │         │ (libcurl)     │
└─────┬────────┘   └─────┬────────┘         └───────────────┘
      ▼                  ▼                         ▼
   ecoWatt.db        config.json/.env       Inverter SIM API
```

## Build on Windows (PowerShell)

Prerequisites
- Visual Studio 2019/2022 with Desktop development with C++
- CMake ≥ 3.16
- vcpkg installed at C:\vcpkg (or provide a custom path)

Install required libraries with vcpkg (one time)
````powershell
& C:\vcpkg\vcpkg.exe install curl[core,ssl]:x64-windows sqlite3:x64-windows nlohmann-json:x64-windows spdlog:x64-windows
````

Option A — Use the provided build script
````powershell
cd cpp
.\n+build.ps1
````

Option B — Manual CMake (from repo root)
````powershell
# Configure (generator will pick MSVC and x64 via -A x64)
cmake -S cpp -B cpp\build -DCMAKE_TOOLCHAIN_FILE="C:\vcpkg\scripts\buildsystems\vcpkg.cmake" -A x64

# Build (Release)
cmake --build cpp\build --config Release --parallel
````

Option B — Manual CMake (from cpp folder)
````powershell
cd cpp
# Important: when inside cpp, use -S . not -S cpp
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE="C:\vcpkg\scripts\buildsystems\vcpkg.cmake" -A x64
cmake --build build --config Release --parallel
````

Common pitfall: Running `cmake -S cpp -B build` while already inside `cpp` fails because it points to a non-existent `cpp/cpp` source path. Use `-S .` when inside the folder.

## Configure

1) Create `cpp\.env` (required)
````dotenv
# Inverter SIM API
INVERTER_API_KEY=REPLACE_WITH_YOUR_KEY
INVERTER_API_BASE_URL=http://20.15.114.131:8080

# Optional overrides
DATABASE_PATH=ecoWatt_milestone2.db
LOG_LEVEL=INFO
LOG_FILE=ecoWatt_milestone2.log
DEFAULT_SLAVE_ADDRESS=17
MAX_RETRIES=3
REQUEST_TIMEOUT_MS=5000
RETRY_DELAY_MS=1000
````

2) Review `cpp\config.json` (already provided)
- Modbus: timeouts/retries, slave address
- Acquisition: polling interval, min registers `[0,1]`
- Storage: memory retention, persistent storage, retention
- API: read/write endpoints, headers (content type, accept)
- Registers: names, units, gains, access

The post-build step copies `.env` and `config.json` beside `EcoWattDevice.exe`.

## Run

After a successful build (Release):
````powershell
cd cpp\build\Release
.
\EcoWattDevice.exe --config "config.json" --env ".env"
````

Optional demo mode (prints periodic status, performs write examples, exports sample files):
````powershell
.
\EcoWattDevice.exe --demo
````

If the process exits immediately with code 1 and logs a configuration error, ensure `.env` contains a valid `INVERTER_API_KEY` and the file is copied to the Release directory.

## Inverter SIM API (per spec)

- Read endpoint: `POST http://20.15.114.131:8080/api/inverter/read`
  - Body: `{ "frame": "<HEX>" }`
  - Response: `{ "frame": "<HEX>" }` (blank on invalid frame)
- Write endpoint: `POST http://20.15.114.131:8080/api/inverter/write`
  - Body: `{ "frame": "<HEX>" }`
  - Response: echoes request on success; exception frame on errors
- Auth: `Authorization: <api key>` header is required

Modbus RTU framing
- Request: `[Slave][Function][Data...][CRC_LSB][CRC_MSB]`
- Response: Read 0x03: `[Slave][0x03][ByteCount][Data...][CRC]`
- Write 0x06 echoes address+value
- Exception: `[Slave][Function|0x80][ExceptionCode][CRC]`
- CRC-16 (Modbus): polynomial 0xA001, initial 0xFFFF, little-endian in frame

## What the code implements

Protocol Adapter (`include/protocol_adapter.hpp`, `src/protocol_adapter.cpp`)
- Builds frames for 0x03 (Read Holding Registers) and 0x06 (Write Single Register)
- Computes/validates CRC, parses success and exception responses
- Uses libcurl to POST JSON `{ "frame": "HEX" }` with headers:
  - `Authorization: <INVERTER_API_KEY>`
  - `Content-Type: application/json`, `Accept: */*`
- Retries on transient HTTP/parse errors up to `max_retries`, with `retry_delay`
- Detects empty or malformed frames and throws typed exceptions; logs at each step

Acquisition Scheduler (`include/acquisition_scheduler.hpp`, `src/acquisition_scheduler.cpp`)
- Configurable polling interval (`acquisition.polling_interval_ms`)
- Ensures minimum registers `[0,1]` (voltage, current) are always polled
- Converts raw to scaled values using register-specific `gain` (scaled = raw / gain)
- Provides callbacks for new samples and errors
- Includes a write path for register 8 (export power percentage, 0–100)

Data Storage (`include/data_storage.hpp`, `src/data_storage.cpp`)
- Memory ring buffers per register (fast recent reads)
- Optional persistent SQLite storage with simple schema
- Combined stats and optional daily cleanup (retention)

Main entry (`src/main.cpp`)
- Loads config + initializes logging
- Tests communication (reads voltage), starts polling, prints periodic status
- Demo mode: shows status updates, writes export power (75% then 50%), exports sample files

## Registers (per spec)

| Addr | Name                          | Access     | Gain | Unit |
|-----:|--------------------------------|------------|-----:|------|
| 0    | Vac1 / L1 Phase voltage       | Read       | 10   | V    |
| 1    | Iac1 / L1 Phase current       | Read       | 10   | A    |
| 2    | Fac1 / L1 Phase frequency     | Read       | 100  | Hz   |
| 3    | Vpv1 / PV1 input voltage      | Read       | 10   | V    |
| 4    | Vpv2 / PV2 input voltage      | Read       | 10   | V    |
| 5    | Ipv1 / PV1 input current      | Read       | 10   | A    |
| 6    | Ipv2 / PV2 input current      | Read       | 10   | A    |
| 7    | Inverter internal temperature | Read       | 10   | °C   |
| 8    | Export power percentage       | Read/Write | 1    | %    |
| 9    | Pac L / Output power          | Read       | 1    | W    |

Note: Gains are divisors for scaling: scaled = raw / gain.

## Validate end-to-end

Quick run
````powershell
cd cpp\build\Release
.
\EcoWattDevice.exe --config "config.json" --env ".env"
# Expect: communication test success, then periodic status, and current readings block
````

Manual API check (optional, requires curl)
````powershell
curl -X POST "http://20.15.114.131:8080/api/inverter/read" `
  -H "accept: */*" `
  -H "Authorization: <your api key>" `
  -H "Content-Type: application/json" `
  -d '{"frame":"110300000002C69B"}'
````

Write test (export power 16 at address 8)
````powershell
curl -X POST "http://20.15.114.131:8080/api/inverter/write" `
  -H "accept: */*" `
  -H "Authorization: <your api key>" `
  -H "Content-Type: application/json" `
  -d '{"frame":"1106000800100B54"}'
````

## Troubleshooting

- Build fails in CMake when already in cpp folder
  - Use `cmake -S . -B build` (not `-S cpp`) when inside `cpp`
- CMake picks Clang on Windows but you don't have it
  - The CMakeLists.txt sets Clang on WIN32: `set(CMAKE_C_COMPILER "clang")` and `set(CMAKE_CXX_COMPILER "clang++")`
  - If Clang/LLVM isn't installed, comment those two lines or let the Visual Studio generator pick MSVC by default
  - Alternatively, configure with `-T ClangCL` if you have LLVM installed via VS components
- Missing API key / immediate exit
  - Ensure `cpp\.env` has a valid `INVERTER_API_KEY` and that `.env` is copied to the Release dir
- HTTP/CURL errors
  - Check network connectivity to `http://20.15.114.131:8080`
  - Verify your API key
  - Increase `REQUEST_TIMEOUT_MS` or `MAX_RETRIES` in `.env` for unstable links
- Empty or malformed frame in response
  - The API returns blank on invalid frames; check function code, addresses, CRC
- Unicode in console (°C)
  - The app sanitizes units for Windows console output (prints `degC`)

## Design notes

- CRC algorithm matches Modbus RTU (polynomial 0xA001, initial 0xFFFF) with LSB-first placement
- Modbus exception responses are parsed and logged with the proper code (01, 02, 03, ...)
- Retries are time-based sleeps between attempts; failures raise typed exceptions
- Scaled values use `scaled = raw / gain` following the provided register table

## Next steps (future milestones)

- Remote configuration updates (runtime changes to acquisition tables)
- Web dashboard for visualization
- Cloud export pipelines
- Extended test suite and CI

---

Course context: EN4440 Embedded Systems Engineering — University of Moratuwa
