# AEGIS GCS

**A**erospace **E**xtensible **G**round **C**ontrol **S**tation — A modular, C++/Qt6-based operator platform for UAS telemetry, mission planning, and real-time visualization.

> Built as a portfolio project demonstrating enterprise-grade application architecture for mission-critical aerospace software.

---

## Overview

AEGIS GCS is a **desktop Ground Control Station** written in **C++20** with **Qt6 Widgets**. It ingests live MAVLink telemetry via UDP and exposes data to a dynamic, dockable UI workspace built on Qt's advanced docking framework. The core differentiator is its **runtime plugin architecture** using `QPluginLoader` — new capability panels (
Telemetry HUD, Map, Mission Editor, Alert Console) are compiled as shared libraries and loaded without recompiling the shell.

This project targets the competencies expected of a senior Aerospace Application Software Engineer:
- **Modular, maintainable architecture** across multiple subsystem interfaces
- **Real-time telemetry** ingestion and multi-threaded data flow via Qt Signals & Slots
- **Operator-centric UI/UX** with configurable, persistent dockable layouts
- **Enterprise-grade plugin SDK** with interface contracts and sandboxed lifecycle
- **Geospatial visualization** (CesiumJS via Qt WebEngine)
- **CI/CD**, automated testing (GoogleTest), and strict interface contracts

---

## Architecture at a Glance

```
┌────────────────────────────────────────────────────────────┐
│                  AEGIS Shell (Qt6 Widgets)                  │
│  ┌──────────┐  ┌──────────┐  ┌──────────────┐  ┌─────────┐ │
│  │ Telemetry│  │Mission   │  │    Map       │  │  Alert  │ │
│  │   HUD    │  │ Editor   │  │  (Cesium)    │  │ Console │ │
│  │ (Plugin) │  │ (Plugin) │  │  (Plugin)    │  │(Plugin) │ │
│  └────┬─────┘  └────┬─────┘  └──────┬───────┘  └────┬────┘ │
│       │             │               │               │       │
├───────┴─────────────┴───────────────┴───────────────┴───────┤
│              Plugin SDK (IPlugin / QPluginLoader)           │
├────────────────────────────────────────────────────────────┤
│                    Core Services                              │
│  • TelemetryBus (thread-safe pub/sub, Qt::QueuedConnection)│
│  • VehicleState (canonical model, QReadWriteLock)            │
│  • PluginHost (runtime discovery, error isolation)           │
├────────────────────────────────────────────────────────────┤
│                   Telemetry I/O                               │
│  • MAVLinkIO (QUdpSocket on dedicated QThread)             │
│  • MavlinkParser (demux → VehicleState updates)            │
│  • LogReplay (.tlog file reader with speed control)          │
└────────────────────────────────────────────────────────────┘
```

---

## Tech Stack

| Layer           | Technology                                         |
|-----------------|----------------------------------------------------|
| Language        | C++20                                              |
| Build System    | CMake ≥3.20                                        |
| UI Framework    | Qt6 Widgets (native C++)                           |
| Plugin System   | Qt `QPluginLoader` + `Q_INTERFACES`               |
| Telemetry       | MAVLink 2 (C library)                               |
| Mapping         | CesiumJS via `QWebEngineView` + `QWebChannel`   |
| State Bus       | Qt Signals & Slots (`Qt::QueuedConnection`)       |
| Threading       | `QThread`, `QReadWriteLock`, `QMutex`              |
| Logging         | Thread-safe `Logger` singleton                     |
| Collections     | `QHash`, `QMultiHash`, `QVector`, `QQueue`         |
| Testing         | GoogleTest + Qt Test                                |
| CI/CD           | GitHub Actions (Linux GCC, Windows MSVC)           |

---

## Repository Structure

```
aegis-gcs/
├── .github/workflows/           # CI/CD (Linux + Windows Qt6 builds)
├── config/
│   └── aegis.json               # Runtime plugin & telemetry config
├── docs/
│   └── architecture.md          # Full TDD with ADRs
├── resources/
│   └── resources.qrc            # Qt resource bundle
├── src/
│   ├── app/
│   │   ├── application.hpp/.cpp # Bootstrapper + service wiring
│   │   └── main.cpp             # Entry point
│   ├── core/
│   │   ├── interfaces/          # IPlugin, ITelemetrySink, ICommandSource
│   │   ├── bus/                 # TelemetryBus (pub/sub, thread-safe)
│   │   ├── state/               # VehicleState (canonical ground truth)
│   │   ├── plugin_host/         # PluginHost, PluginMeta
│   │   └── types/               # Common data structures + QMetaType regs
│   ├── telemetry/
│   │   ├── mavlink_io.hpp/.cpp  # UDP RX/TX on background thread
│   │   ├── parsers.hpp/.cpp     # MAVLink demux → State mutations
│   │   ├── replay/              # LogReplay (.tlog playback)
│   │   └── types/               # Normalized message envelope
│   ├── ui/
│   │   ├── main_window.hpp/.cpp # Dockable shell with menu/toolbar/status
│   │   ├── dock_manager.hpp/.cpp# Plugin widget injection + tabify
│   │   ├── theme/               # QSS dark theme + safety tokens
│   │   └── widgets/             # Connection bar, status bar
│   ├── plugins/
│   │   ├── telemetry_hud/       # Real-time attitude / battery HUD
│   │   ├── mission_editor/      # Waypoint table + upload capability
│   │   └── alert_console/       # Severity-colored alert log
│   └── utils/
│       ├── logging.hpp/.cpp       # Structured crash-safe logger
│       ├── ring_buffer.hpp      # Fixed-size telemetry history
│       └── thread_pool.hpp/.cpp # QtConcurrent wrapper
├── tests/
│   ├── core/                    # TelemetryBus, VehicleState GTests
│   └── utils/                   # RingBuffer tests
├── CMakeLists.txt               # Root CMake (superbuild)
└── README.md                    # This file
```

---

## Quick Start

### Prerequisites

| Component | Version | Notes |
|-----------|---------|-------|
| C++ Compiler | MSVC 2019+, GCC 11+, Clang 14+ | C++20 required |
| CMake | ≥ 3.20 | |
| Qt6 | 6.6+ | Core, Widgets, Network, Qml, Concurrent |
| Qt6 WebEngine | 6.6+ | **Optional** — only needed for map plugin |
| GoogleTest | latest | **Optional** — only needed for tests |

### Windows (Visual Studio 2022)

If you don't have Qt6 installed, use **aqtinstall** (no Qt account required):

```powershell
# 1. Install aqtinstall (requires Python)
python -m pip install aqtinstall

# 2. Download Qt6.8.2 for MSVC 2022 (~2.5GB with WebEngine; skip -m qtwebengine if you don't need the map plugin)
python -m aqt install-qt windows desktop 6.8.2 win64_msvc2022_64 --outputdir C:\Qt -m qtwebengine

# 3. Configure with Qt path
mkdir build
cmake -B build -S . -DCMAKE_PREFIX_PATH="C:/Qt/6.8.2/msvc2022_64" -DAEGIS_BUILD_TESTS=OFF

# 4. Build (Release)
cmake --build build --config Release --parallel

# 5. Run
.\build\Release\aegis.exe
```

> **No manual DLL copying.** `windeployqt` runs automatically as a post-build step. Custom plugins are also copied to `build/Release/plugins/` automatically.

### Linux (Ubuntu/Debian)

```bash
# Install Qt6 from package manager
sudo apt update
sudo apt install -y cmake ninja-build build-essential \
    qt6-base-dev libqt6widgets6

# Optional: WebEngine for map plugin
sudo apt install -y qt6-webengine-dev

# Optional: tests
sudo apt install -y libgtest-dev

# Build
mkdir build && cd build
cmake .. -GNinja -DCMAKE_BUILD_TYPE=Release -DAEGIS_BUILD_TESTS=ON
cmake --build . --parallel
ctest --output-on-failure
```

### macOS (Homebrew)

```bash
# Install dependencies
brew install cmake qt@6

# Build
mkdir build && cd build
cmake .. -DCMAKE_PREFIX_PATH=$(brew --prefix qt@6) \
         -DCMAKE_BUILD_TYPE=Release \
         -DAEGIS_BUILD_TESTS=OFF
cmake --build . --parallel

# Run
./aegis
```

### Run
```bash
# Windows
.\build\Release\aegis.exe

# Linux / macOS
./build/aegis
```

### What happens automatically after each build

| Step | Trigger | Output |
|------|---------|--------|
| `windeployqt` (Windows) | `aegis.exe` links successfully | Qt6 DLLs + platform plugins copied to `Release/` |
| Plugin copy (all platforms) | Each plugin links successfully | Custom plugins copied to executable's `plugins/` dir |

After any source change, simply rebuild and run — no extra steps:

```bash
# Windows
cmake --build build --config Release --parallel
.\build\Release\aegis.exe

# Linux / macOS
cmake --build build --parallel
./build/aegis
```

### Troubleshooting

| Error | Fix |
|-------|-----|
| `Qt6 not found` during configure | Set `-DCMAKE_PREFIX_PATH` to your Qt installation, e.g. `C:/Qt/6.8.2/msvc2022_64` |
| `Qt platform plugin could not be initialized` | Missing `platforms/*.dll`. The post-build `windeployqt` step handles this automatically; rebuild from a clean `build/` directory |
| Plugins not loading at runtime | Verify `plugins/` directory exists next to the executable and contains `telemetry_hud.dll` (or `.so`/`.dylib`) |
| `GoogleTest not found` | Tests are skipped automatically. Install GTest to enable them: `vcpkg install gtest` (Windows) or `sudo apt install libgtest-dev` (Linux) |

---

## Plugin Development

Plugins are **shared libraries** (`.so`/`.dll`) implementing `IPlugin`.

```cpp
// my_plugin.hpp
#include "core/interfaces/iplugin.hpp"

class MyPlugin : public aegis::core::IPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "com.aegis.gcs.IPlugin/1.0" FILE "my_plugin.json")
    Q_INTERFACES(aegis::core::IPlugin)
public:
    bool initialize(TelemetryBus* bus, VehicleState* state,
                    const QVariantMap& config) override;
    QWidget* widget() override;
    void shutdown() override;
    QString name() const override { return "My Plugin"; }
    // ...
};
```

Copy the compiled library into the `plugins/` directory — AEGIS discovers it automatically via `QPluginLoader` metadata.

---

## Key Design Decisions

| Decision | Rationale |
|----------|-----------|
| **C++20** | Required for real-time guarantees; job description lists C++ as core language alongside C# |
| **Qt6 Widgets over QML** | Defense/aerospace GCS ecosystems are predominantly Widgets-based; matches enterprise codebase patterns |
| **QPluginLoader over dynamic `dlopen`** | Qt-native error handling, metadata introspection, platform abstraction |
| **Qt Signals for cross-thread bus** | Automatic `QueuedConnection` marshaling — eliminates manual condition-variable boilerplate |
| **QReadWriteLock on VehicleState** | Allows concurrent reads by multiple plugins without serialization bottlenecks |
| **Normalized types + MAVLink abstraction** | Enables future DDS/ZeroMQ backend swap with zero UI or plugin changes |
| **GoogleTest over Qt Test alone** | Aerospace/defense industry standard; better CI tooling integration |

---

## Roadmap

- [x] C++20 / Qt6 CMake project scaffolding
- [x] Core services: TelemetryBus, VehicleState, PluginHost
- [x] Plugin SDK with QPluginLoader + 3 example plugins
- [x] Qt6 dockable UI shell with dark theme
- [x] MAVLink I/O abstraction + parser stubs
- [x] GoogleTest harness + CI/CD
- [ ] Full MAVLink C library integration (heartbeat, attitude, position)
- [ ] CesiumJS mapping bridge via QWebChannel
- [ ] SIL/HIL Docker Compose environment
- [ ] DDS / ZeroMQ alternate telemetry backend
- [ ] Mission file export (QGroundControl .plan compatibility)

---

## License

MIT — Portfolio project.
