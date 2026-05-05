# AEGIS GCS

**A**erospace **E**xtensible **G**round **C**ontrol **S**tation — a modular, C++/Qt6-based operator platform for UAS telemetry, mission planning, and real-time visualization.

> Portfolio project focused on plugin architecture, telemetry flow, and desktop operator tooling.

---

## Overview

AEGIS GCS is a **desktop Ground Control Station** written in **C++20** with **Qt6 Widgets**. It ingests MAVLink telemetry over UDP, maintains a thread-safe shared vehicle state model, and exposes functionality through **runtime-loaded plugins** using `QPluginLoader`.

### Current implemented capabilities

- Runtime plugin discovery and loading
- Telemetry HUD plugin
- Alert Console plugin
- Mission Editor plugin (waypoint table and command-source scaffold)
- Native 2D Map View plugin with OpenStreetMap raster tiles
- Dummy telemetry generator for offline UI testing
- MAVLink log replay service (binary frame replay into the live parser path)
- Dark Qt Widgets operator shell
- Windows Qt runtime deployment via `windeployqt`
- Growing unit and integration test suite (GoogleTest + Qt Test)

### Current map implementation

The map plugin currently uses a **native Qt `QGraphicsView` / `QGraphicsScene` 2D map view**.

- It works **without Qt WebEngine**
- It shows a grid, vehicle marker, heading line, and track history
- It is injected as the **main central view** in the application shell

`Qt WebEngine` / `Cesium` is **not the active map path today**. It is still a future enhancement.

---

## Architecture at a Glance

```text
┌────────────────────────────────────────────────────────────┐
│                  AEGIS Shell (Qt6 Widgets)                │
│                                                            │
│   Central View: Map View Plugin (Qt Graphics / 2D)        │
│                                                            │
│   Right Docks: Telemetry HUD / Mission Editor / Alerts    │
├────────────────────────────────────────────────────────────┤
│              Plugin SDK (IPlugin / QPluginLoader)         │
├────────────────────────────────────────────────────────────┤
│ Core Services                                             │
│  • TelemetryBus                                           │
│  • VehicleState                                           │
│  • PluginHost                                             │
│  • Shared Types (AttitudeData, PositionData, BatteryData,│
│    SystemState, VehicleCommand)                           │
├────────────────────────────────────────────────────────────┤
│ Telemetry Layer                                           │
│  • MavlinkIO                                              │
│  • MavlinkParser                                          │
│  • LogReplay                                              │
└────────────────────────────────────────────────────────────┘
```

### Telemetry Pipeline

Live MAVLink frames flow through a dedicated worker thread and are delivered to the main thread via Qt queued connections:

```text
UDP Socket → MavlinkIO → MavlinkParser → VehicleState + TelemetryBus → UI/Plugins
```

**Important design decision:** socket bind success is **not** treated as a vehicle connection. A vehicle is considered connected only after a valid MAVLink heartbeat is received. If no heartbeat arrives within the configured timeout, the system emits disconnected.

---

## Tech Stack

| Layer | Technology |
|---|---|
| Language | C++20 |
| Build System | CMake ≥ 3.20 |
| UI Framework | Qt6 Widgets |
| Plugin System | Qt `QPluginLoader` + `Q_INTERFACES` |
| Telemetry | MAVLink 2 C headers |
| Mapping | Native Qt `QGraphicsView` fallback |
| State Bus | Qt Signals & Slots |
| Threading | `QThread`, `QReadWriteLock`, `QMutex` |
| Logging | Custom thread-safe logger |
| Testing | GoogleTest + Qt Test |
| CI/CD | GitHub Actions (Linux + Windows build/test jobs) |

---

## Repository Structure

```text
aegis-gcs/
├── config/
│   └── aegis.json
├── resources/
│   └── resources.qrc
├── src/
│   ├── app/
│   │   ├── application.hpp
│   │   └── application.cpp
│   ├── core/
│   │   ├── bus/
│   │   ├── interfaces/
│   │   ├── plugin_host/
│   │   ├── state/
│   │   └── types/
│   ├── plugins/
│   │   ├── alert_console/
│   │   ├── map_view/
│   │   ├── mission_editor/
│   │   └── telemetry_hud/
│   ├── telemetry/
│   ├── ui/
│   │   ├── theme/
│   │   └── widgets/
│   └── utils/
├── tests/
├── third_party/
│   └── mavlink/
├── CMakeLists.txt
└── README.md
```

---

## Build and Run

### Requirements

| Component | Notes |
|---|---|
| CMake | 3.20+ |
| Compiler | MSVC 2022 / GCC 11+ / Clang 14+ |
| Qt6 | Core, Widgets, Network, Qml, Concurrent |
| Qt WebEngine | Optional, not required for current map plugin |
| GoogleTest | Optional |

### Windows (Visual Studio 2022)

```powershell
cmake -B build -S . -DCMAKE_PREFIX_PATH="C:/Qt/6.8.2/msvc2022_64" -DAEGIS_BUILD_TESTS=OFF
cmake --build build --config Release --parallel
.\build\Release\aegis.exe
```

If you are using a separate build tree:

```powershell
cmake -B build_cesium -S . -DCMAKE_PREFIX_PATH="C:/Qt/6.8.2/msvc2022_64" -DAEGIS_BUILD_TESTS=OFF
cmake --build build_cesium --config Release --parallel
.\build_cesium\Release\aegis.exe
```

### Linux

```bash
cmake -B build -S . -GNinja -DCMAKE_BUILD_TYPE=Release -DAEGIS_BUILD_TESTS=OFF
cmake --build build --parallel
./build/aegis
```

### macOS

```bash
cmake -B build -S . -DCMAKE_PREFIX_PATH=$(brew --prefix qt@6) -DCMAKE_BUILD_TYPE=Release -DAEGIS_BUILD_TESTS=OFF
cmake --build build --parallel
./build/aegis
```

### Building with Tests

```powershell
cmake -B build -S . -DCMAKE_PREFIX_PATH="C:/Qt/6.8.2/msvc2022_64" -DAEGIS_BUILD_TESTS=ON
cmake --build build --config Release --parallel
ctest --test-dir build --output-on-failure -C Release
```

---

## Runtime Notes

### Plugin loading

The executable searches for plugins in this order:

1. `<applicationDir>/plugins`
2. `<currentWorkingDir>/plugins`
3. any extra paths from `config/aegis.json`

On Windows, plugins are built into:

```text
build/Release/plugins/
```

or, for alternate build trees:

```text
build_cesium/Release/plugins/
```

### Default startup behavior

- Map View is autostarted
- Map View is shown as the **main center panel**
- Dummy telemetry starts automatically for offline testing
- Status bar shows connection + vehicle mode/armed state

### Current UI Features

- Dark operator theme via `ThemeEngine`
- File menu for opening replay logs
- Connect menu and toolbar action for UDP MAVLink
- Status bar connection indicator
- Vehicle armed/mode display derived from heartbeat state
- Plugin injection and basic dock/tab management

---

## Configuration

Runtime configuration lives in:

```text
config/aegis.json
```

### Implemented Config Domains

| Domain | Purpose |
|---|---|
| `pluginPaths` | Additional directories for plugin discovery |
| `autostartPlugins` | Plugin IDs to load at startup |
| `telemetry` | UDP bind address, bind port, heartbeat timeout |
| `logging` | Minimum log level and log file path |
| `dummyTelemetry` | Offline/demo telemetry generator enablement and interval |
| `plugins` | Per-plugin settings passed to each plugin during initialization |

### Example Snippet

```json
{
  "telemetry": {
    "bindAddress": "0.0.0.0",
    "bindPort": 14550,
    "heartbeatTimeoutMs": 3000
  },
  "dummyTelemetry": { "enabled": true, "intervalMs": 100 },
  "plugins": {
    "aegis.plugins.map_view": {
      "zoom": 15,
      "tileUrlTemplate": "https://tile.openstreetmap.org/%1/%2/%3.png"
    },
    "aegis.plugins.alert_console": {
      "maxItems": 250,
      "showTimestamps": true
    }
  }
}
```

---

## Troubleshooting

| Problem | Fix |
|---|---|
| `Qt6 not found` | Set `-DCMAKE_PREFIX_PATH` to your Qt installation |
| `Discovered 0 plugins` | Verify `Release/plugins/*.dll` exists and `config/aegis.json` is not pointing only at the wrong folder |
| App opens but plugins are missing | Launch the executable from the build output directory and ensure plugin DLLs are next to it in `plugins/` |
| `LNK1104 cannot open aegis.exe` | Close the running app before rebuilding |
| `Qt6WebEngineWidgets not found` | Safe to ignore for the current native map implementation |
| Blank center panel | Rebuild after config/layout fixes; the active implementation now injects the map as the main central widget |

---

## MAVLink Coverage

The parser currently decodes the following MAVLink 2 messages:

| MAVLink Message | AEGIS Output |
|---|---|
| `HEARTBEAT` | System ID/component, base mode, custom mode, armed state, system status |
| `ATTITUDE` | Roll, pitch, yaw, angular rates |
| `GLOBAL_POSITION_INT` | Latitude, longitude, altitude, relative altitude, velocity, heading |
| `BATTERY_STATUS` | Remaining percentage, voltage, current |
| `MISSION_CURRENT` | Current mission index |
| `STATUSTEXT` | Alert severity mapping and alert console output |

## Map Implementation Details

The map plugin uses native Qt graphics (no embedded browser):

| Feature | Implementation |
|---|---|
| Tile provider | Configurable URL template, defaulting to `https://tile.openstreetmap.org/%1/%2/%3.png` |
| Projection | Web Mercator tile coordinate formula |
| Networking | `QNetworkAccessManager` and `QNetworkReply` |
| Rendering | `QGraphicsView`, `QGraphicsScene`, pixmap tile items, path overlay, marker overlay |
| Zoom | Changes OSM tile zoom level and refreshes tile set |

> **Operational note:** OSM tile usage requires respecting the OpenStreetMap tile policy. A commercial deployment should add tile caching, provider attribution, rate limiting, and preferably a dedicated commercial tile provider or offline MBTiles support.

## Testing

Current automated test targets:

| Test Target | Coverage |
|---|---|
| `test_telemetry_bus` | Signal delivery, topic pub/sub, outbound commands, command responses |
| `test_vehicle_state` | Thread-safe state update/getter behavior |
| `test_ring_buffer` | Fixed-size telemetry history behavior |
| `test_parsers` | HEARTBEAT, ATTITUDE, GLOBAL_POSITION_INT, BATTERY_STATUS |
| `test_log_replay` | Replay step and playback completion/progress |
| `test_mavlink_io` | UDP heartbeat connection and timeout disconnection |

## Plugin Development

Plugins are shared libraries implementing `IPlugin`.

```cpp
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
};
```

On Windows, place the built plugin in the executable's `plugins/` directory.

---

## Commercial-Grade Readiness Snapshot

| Area | Current State | Commercial Next Step |
|---|---|---|
| Architecture | Modular and plugin-based | Stabilize plugin SDK/ABI and add service registry |
| Telemetry | Selected MAVLink messages decoded | Complete mission, parameter, command ACK, signing, and multi-vehicle support |
| Map | Real OSM online tile layer | Add offline cache, provider attribution, commercial provider, and geofence editing |
| Safety | Heartbeat timeout and alerts scaffold | Add full failsafe rules, command confirmation, and audit logging |
| Testing | Growing unit/integration test suite | Add fuzzing, UI smoke tests, sanitizers, replay regression corpus, and hardware-in-loop tests |
| Deployment | Build and Windows runtime deployment scaffold | Add installer, updater, crash reporter, license management, and diagnostics export |

## Current Limitations

- The map uses native Qt/OpenStreetMap raster tiles, not a 3D globe or Cesium bridge
- Online map tiles require network access unless an offline cache/provider is added
- MAVLink coverage is still partial compared to mature GCS products
- Mission editing is basic and does not yet implement the full MAVLink mission protocol
- Saved advanced dock layout restore is currently intentionally simplified to avoid hiding active plugin panels

---

## Roadmap

### Completed Foundation

- [x] Qt6/C++20 project scaffold
- [x] Runtime plugin architecture
- [x] Telemetry bus + shared vehicle state
- [x] Native Qt map plugin
- [x] OpenStreetMap raster tile support
- [x] Windows plugin deployment into `Release/plugins`
- [x] Config-driven telemetry/logging/plugin settings
- [x] Heartbeat-based connection state and timeout handling
- [x] Initial parser/replay/UDP test coverage

### Phase 1 — Commercial-Grade Foundation

- [ ] Formal connection state machine: socket bound, vehicle discovered, heartbeat alive, degraded, disconnected, reconnecting
- [ ] Config schema validation and migration system
- [ ] Structured rotating logs and diagnostic bundle export
- [ ] Crash reporting and recovery workflow
- [ ] MAVLink command ACK handling
- [ ] Failsafe alerting: heartbeat lost, GPS lost, low battery, EKF unhealthy, RC lost, geofence breach
- [ ] Command confirmation workflow for arm/disarm, mission upload, RTL, and emergency commands
- [ ] Tamper-evident command/event audit log
- [ ] Read-only/demo/operator modes separated from real control mode
- [ ] CI hardening with static analysis, sanitizers, and replay regression tests

### Phase 2 — Real GCS Functionality

- [ ] Full MAVLink mission protocol: download, upload, clear, partial update, ACK handling
- [ ] Parameter protocol: download, edit, search/filter, diff, import/export
- [ ] Mission planning tools: drag/drop waypoints, survey grid, corridor scan, orbit/circle, terrain following
- [ ] Mission validation: altitude limits, geofence violations, battery estimate, link/range estimate
- [ ] Geofence and rally point editor
- [ ] Better mode decoding for PX4 and ArduPilot
- [ ] Flight log recording and replay timeline
- [ ] Telemetry graphing dashboard and event timeline
- [ ] Export telemetry to CSV/JSON and generate flight reports

### Phase 3 — Map and Navigation

- [ ] Tile disk cache
- [ ] Offline maps and MBTiles support
- [ ] Multiple map providers: OSM, satellite, terrain, enterprise/custom tile servers
- [ ] Map provider attribution and rate-limit controls
- [ ] Mouse panning, zoom-to-vehicle, and follow-vehicle toggle
- [ ] Home position marker, vehicle trail persistence, and return-to-home visualization
- [ ] Distance/bearing measurement tool
- [ ] No-fly zone and ADS-B/traffic overlays
- [ ] Optional WebEngine/Cesium 3D globe integration

### Phase 4 — UI/UX and Operator Workflow

- [ ] Workspace presets: pilot, mission planner, diagnostics, replay analysis
- [ ] Robust dock layout persistence and reset workflow
- [ ] Plugin manager and plugin load/unload UI
- [ ] Command palette and keyboard shortcuts
- [ ] Audio alerts and notification center
- [ ] Better status bar: GPS fix, satellites, battery, link quality, armed state, flight mode
- [ ] First-run setup wizard and vehicle/link setup wizard
- [ ] High-DPI polish, theme customization, and accessibility pass

### Phase 5 — Security and Enterprise Readiness

- [ ] User accounts and roles: viewer, operator, maintainer, admin
- [ ] Permission system for vehicle commands and plugins
- [ ] Signed plugins and plugin allowlist
- [ ] MAVLink signing support
- [ ] Encrypted secrets/config storage
- [ ] Secure auto-update channel
- [ ] License activation, offline licensing, and edition gating
- [ ] Installer, portable mode, and enterprise deployment profile

### Phase 6 — Architecture and Extensibility

- [ ] Stable plugin SDK and plugin ABI/version compatibility checks
- [ ] Service registry for plugins
- [ ] Typed event bus and stronger command/telemetry domain models
- [ ] Multi-vehicle state model and fleet view
- [ ] Separate telemetry, mission, map, command, and replay services
- [ ] Dependency injection for testability
- [ ] Headless core mode for automation/integration testing
- [ ] Remote API: WebSocket, REST, or gRPC

### Phase 7 — Differentiators

- [ ] AI-assisted diagnostics and anomaly detection
- [ ] Mission risk scoring
- [ ] Battery sag, vibration, GPS drift, and link instability detection
- [ ] Cloud sync and collaboration mode
- [ ] Advanced analytics and post-flight reports

---

## License

MIT
