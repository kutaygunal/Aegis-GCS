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
- Mission Editor plugin
- Native 2D Map View plugin
- Dummy telemetry generator for offline UI testing
- Dark Qt Widgets operator shell
- Windows Qt runtime deployment via `windeployqt`

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
├────────────────────────────────────────────────────────────┤
│ Telemetry Layer                                           │
│  • MavlinkIO                                              │
│  • MavlinkParser                                          │
│  • LogReplay                                              │
└────────────────────────────────────────────────────────────┘
```

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
| Testing | GoogleTest |
| CI/CD | GitHub Actions |

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

---

## Configuration

Runtime configuration lives in:

```text
config/aegis.json
```

Important keys:

- `pluginPaths`
- `autostartPlugins`
- telemetry bind configuration
- logging configuration

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

## Current Limitations

- The map is currently a **2D native Qt scene**, not a real tiled globe or Cesium bridge
- MAVLink parsing is still partial / scaffolded
- Mission editing is basic
- Saved advanced dock layout restore is currently intentionally simplified to avoid hiding active plugin panels

---

## Roadmap

- [x] Qt6/C++20 project scaffold
- [x] Runtime plugin architecture
- [x] Telemetry bus + shared vehicle state
- [x] Native 2D map plugin
- [x] Windows plugin deployment into `Release/plugins`
- [ ] Full MAVLink integration
- [ ] Real map tiles or WebEngine/Cesium integration
- [ ] Mission upload/download refinement
- [ ] Better dock/workspace persistence
- [ ] Additional automated tests

---

## License

MIT
