# AEGIS GCS — Technical Design Document

**Version:** 1.0  
**Date:** 2026-05-01  
**Author:** Applicant Portfolio Project  
**Classification:** Internal / Portfolio

---

## 1. Purpose & Scope

This document defines the architecture, design patterns, and interface contracts for **AEGIS GCS**, a modular, Qt-based Ground Control Station (GCS) for UAS operations. It serves two purposes: (1) drive implementation of the portfolio project, and (2) demonstrate the architectural rigor expected of a senior aerospace application software engineer — particularly around **modularity**, **real-time data handling**, **operator-centric design**, and **enterprise-grade integration**.

---

## 2. System Context

AEGIS GCS operates as the primary operator-facing software in the UAS ground system ecosystem.

```
┌──────────────────────────────────────────────────────────────────┐
│                        GROUND SEGMENT                            │
│  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐      │
│  │   Mission    │    │    AEGIS     │    │   Log &      │      │
│  │   Planning   │◄──►│     GCS      │◄──►│  Analytics   │      │
│  │   Service    │    │   (This)     │    │   Service    │      │
│  └──────────────┘    └──────┬───────┘    └──────────────┘      │
│                             │ MAVLink / DDS                      │
│                    ┌────────┴────────┐                          │
│                    │    Data Link    │                          │
│                    └────────┬────────┘                          │
└───────────────────────────┬────────────────────────────────────┘
                            │ RF / IP
┌───────────────────────────┼────────────────────────────────────┐
│                        AIR SEGMENT                              │
│                  ┌────────┴────────┐                            │
│                  │   Flight Controller│←── Autopilot (PX4/ArduPilot)│
│                  │   (MAVLink)       │                            │
│                  └─────────────────┘                            │
└──────────────────────────────────────────────────────────────────┘
```

**Operator Personas:**
- **Mission Commander:** Monitors overall mission state, multi-vehicle status, and geospatial situational awareness.
- **Vehicle Operator (VO):** Directly controls a single vehicle via manual override, telemetry HUD, sensor feeds.
- **Test Engineer:** Uses log replay, SIL/HIL integration, and automated mission scenario validation.

---

## 3. Architectural Drivers

| ID | Driver | Priority | Rationale |
|----|--------|----------|-----------|
| AD-1 | **Operator Safety** | Critical | Software failures must not induce unsafe vehicle states; telemetry degradation must be immediately obvious. |
| AD-2 | **Modularity** | High | New vehicles, sensors, and missions require capability expansion without core recompilation. |
| AD-3 | **Real-Time Performance** | High | Telemetry must propagate from air segment to UI within <100ms target (human perception + control loop). |
| AD-4 | **Cross-Platform** | Medium | Development on Linux; target deployment on Windows and Linux operator stations. |
| AD-5 | **Testability** | High | All subsystems must be unit-testable in isolation; integration tests must run headlessly in CI. |

---

## 4. Architecture Style & Patterns

### 4.1 Hexagonal / Ports-and-Adapters
The core (`TelemetryBus`, `VehicleState`, `PluginHost`) exposes **ports** (interfaces). Adapters connect the core to the outside world:
- **MAVLink Adapter:** UDP/TCP MAVLink → normalized vehicle state.
- **Plugin Adapter:** Runtime-loaded Qt widgets that adapt operator intent back to the core.
- **Storage Adapter:** SQLite `.tlog` read/write.

### 4.2 Model-View-ViewModel (MVVM) via Qt Signals
- **Model:** `VehicleState` — canonical ground truth of all vehicle parameters.
- **ViewModel:** `TelemetryBus` — normalized message bus; Plugins subscribe instead of polling.
- **View:** Qt Widgets / QML — bound to state changes via typed `pyqtSignal` / `Signal` emissions.

### 4.3 Plugin Architecture
- Runtime discovery via `importlib` + ABC registration.
- Sandboxed lifecycle: `initialize(state, bus) → run → shutdown()`.
- Qt Plugin SDK provides dockable frame, theme tokens, and telemetry subscriptions.

---

## 5. Subsystem Design

### 5.1 Core Services (`src/aegis/core/`)

#### 5.1.1 TelemetryBus
**Responsibility:** Thread-safe, typed pub/sub for all telemetry, commands, and system events.

```python
class TelemetryBus(QObject):
    # Typed signals emitted on the Qt event loop
    attitudeChanged = Signal(AttitudeData)
    positionChanged = Signal(PositionData)
    heartbeatReceived = Signal(HeartbeatData)
    alertRaised = Signal(AlertLevel, str)
    commandResponse = Signal(int, bool, str)  # command_id, success, message

    def subscribe(self, topic: str, callback: Callable): ...
    def publish(self, topic: str, data: Any): ...
    def post_command(self, cmd: VehicleCommand): ...
```

**Design Decisions:**
- **Qt Signals over raw callbacks:** Automatic cross-thread marshaling via `Qt::QueuedConnection`. Prevents UI deadlocks when telemetry arrives on the MAVLink I/O thread.
- **Topic strings for extensibility:** New vehicle types introduce new topics without changing the bus interface.

#### 5.1.2 VehicleState
**Responsibility:** Canonical state model for the active vehicle(s). Thread-safe via `QReadWriteLock`.

```python
class VehicleState(QObject):
    stateChanged = Signal(StateDomain, Any)

    @property
    def attitude(self) -> Optional[AttitudeData]: ...
    @property
    def position(self) -> Optional[PositionData]: ...
    @property
    def mission_items(self) -> List[MissionItem]: ...
    @property
    def system_status(self) -> SystemStatus: ...
```

#### 5.1.3 PluginHost
**Responsibility:** Runtime plugin discovery, loading, lifecycle management, and sandbox error isolation.

```python
class PluginHost(QObject):
    pluginLoaded = Signal(str)      # plugin_id
    pluginUnloaded = Signal(str)
    pluginCrashed = Signal(str, Exception)

    def load_plugin(self, path: Path) -> IPlugin: ...
    def unload_plugin(self, plugin_id: str): ...
    def enumerate_plugins(self) -> List[PluginMeta]: ...
```

**Error Isolation:** Each plugin runs in its own QObject hierarchy; uncaught exceptions in a plugin slot do not crash the shell.

### 5.2 Telemetry I/O (`src/aegis/telemetry/`)

#### 5.2.1 MAVLinkIO
- **Transport:** UDP (primary), TCP fallback for ground-to-air links.
- **Threading:** Dedicated `QThread` for socket I/O. Messages parsed with `pymavlink`.
- **Rate Decoupling:** MAVLink thread pushes to `TelemetryBus`; UI thread consumes. No direct socket access from UI.

#### 5.2.2 LogReplay
- Reads `.tlog` (MAVLink binary log) or `.ulg`/`.bin` conversions.
- Emits messages into the same `TelemetryBus` at configurable speed (1x, 2x, 10x) or step mode for debugging.
- Timestamp interpolation to simulate real-time flow.

### 5.3 UI Shell (`src/aegis/ui/`)

#### 5.3.1 MainWindow
- Built on **PySide6 (Qt 6)**.
- Uses `QDockWidget` system for draggable, saveable layouts.
- Central widget reserved for the primary map; auxiliary plugins dock to sides/bottom.
- **Layout Persistence:** `QSettings` saves/restore geometry on clean exit.

#### 5.3.2 DockManager
- Wraps `QMainWindow` dock areas.
- Handles plugin widget injection: `dock_manager.inject(plugin.widget(), allowed_areas)`.
- Tabifies related plugins automatically (e.g., all debug consoles tab together).

#### 5.3.3 Theme Engine
- QSS (Qt StyleSheets) + JSON token definitions.
- **Safety-critical color coding:** Red/amber/green states are locked tokens to prevent operator misinterpretation.
- Dark mode default (reduces eye strain in dim operator stations).

### 5.4 Mapping (`src/aegis/mapping/`)

- **Engine:** CesiumJS hosted inside `QWebEngineView` for 3D globe + terrain.
- **Bidirectional Bridge:** `QWebChannel` exposes Qt objects to JavaScript for sending vehicle position updates.
- **Offline Fallback:** MBTiles via `QGIS` or raster tile caching for denied/disconnected environments.
- **Layers:** Vehicle track (polyline), waypoints (billboards), geofences (corridors), sensor FOV cones.

### 5.5 Plugin SDK (`src/aegis/core/interfaces.py`)

```python
class IPlugin(ABC, QObject):
    name: ClassVar[str]
    version: ClassVar[str]
    author: ClassVar[str]
    category: ClassVar[str]  # "Flight", "Planning", "Diagnostics", "Mission"

    @abstractmethod
    def initialize(self, bus: TelemetryBus, state: VehicleState, config: dict) -> bool: ...

    @abstractmethod
    def widget(self) -> QWidget: ...

    @abstractmethod
    def shutdown(self) -> None: ...

class ITelemetrySink(IPlugin):
    """Plugin that primarily consumes telemetry (e.g., HUD, Map)."""
    pass

class ICommandSource(IPlugin):
    """Plugin that issues commands (e.g., Mission Editor, Joystick)."""
    @abstractmethod
    def can_issue(self, cmd: VehicleCommand) -> bool: ...
```

---

## 6. Data Flow

### 6.1 Real-Time Telemetry Path (Hot Path)

```
MAVLink UDP Stream
       │
       ▼
┌──────────────┐   Parse   ┌──────────────┐
│  MAVLinkIO   │──────────►│ TelemetryBus │
│   (Thread)   │  mavutil  │  (Qt Thread) │
└──────────────┘           └──────┬───────┘
                                  │ Qt Signals
          ┌───────────────────────┼───────────────────────┐
          ▼                       ▼                       ▼
   ┌────────────┐         ┌────────────┐         ┌────────────┐
   │ VehicleState│         │   HUD      │         │   Map      │
   │  (Model)   │         │  Plugin    │         │  Plugin    │
   └────────────┘         └────────────┘         └────────────┘
```

**Latency Budget:**
- Network I/O → Parse: ~5ms (local SITL)
- Bus dispatch (queued): ~1-5ms
- UI slot execution: ~16ms (60Hz cap)
- **Total target: <30ms end-to-end**

### 6.2 Command Path (Operator → Vehicle)

```
Operator Input (Click / Hotkey)
       │
       ▼
┌──────────────┐
│   ICommand   │
│   Source     │
└──────┬───────┘
       │ VehicleCommand envelope
       ▼
┌──────────────┐
│ TelemetryBus │
└──────┬───────┘
       │
       ▼
┌──────────────┐   MAVLink encode   ┌──────────────┐
│   MAVLinkIO  │──────────────────►│ UDP TX Socket│
└──────────────┘                   └──────────────┘
```

Command acknowledgment is tracked via `commandResponse` signal.

---

## 7. Interface Contracts

### 7.1 MAVLink Interface (Inbound)

| Message | Frequency | Mapping |
|---------|-----------|---------|
| `HEARTBEAT` | 1 Hz | `system_status`, `vehicle_type` |
| `ATTITUDE` | 10-50 Hz | `roll`, `pitch`, `yaw`, `rates` |
| `GLOBAL_POSITION_INT` | 5-10 Hz | `lat`, `lon`, `alt`, `relative_alt` |
| `MISSION_CURRENT` | On change | Active waypoint index |
| `BATTERY_STATUS` | 1 Hz | Remaining %, voltage, current |
| `STATUSTEXT` | Async | `alertRaised` signal |

### 7.2 Plugin Interface (Outbound to Core)

```json
{
  "plugin_manifest": {
    "id": "aegis.plugins.mission_editor",
    "name": "Mission Editor",
    "version": "2.1.0",
    "entry_point": "mission_editor:MissionEditorPlugin",
    "permissions": ["read:mission", "write:mission", "command:nav"],
    "min_api_version": "1.0"
  }
}
```

---

## 8. Deployment & DevOps

### 8.1 CI/CD Pipeline (GitHub Actions)

```yaml
on: [push, pull_request]
jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-python@v5
        with: { python-version: "3.11" }
      - run: pip install -r requirements-dev.txt
      - run: pytest tests/unit --cov=src/aegis --cov-report=xml
      - run: mypy src/aegis --strict
      - run: ruff check src/ tests/
```

### 8.2 Packaging
- **Development:** Editable install via `pip install -e .`
- **Distribution:** PyInstaller or `briefcase` for single-file Windows/Linux executable.
- **Containerized SITL:** Docker Compose spins up PX4 + Gazebo + AEGIS headless for CI integration tests.

---

## 9. Risk & Mitigation

| Risk | Impact | Mitigation |
|------|--------|------------|
| Qt signal saturation at high msg rates | UI freeze | Rate-limit UI updates; decouple parser from view via throttled signals |
| Plugin crashes shell | Availability | Plugin sandboxing; each in its own `QObject` tree; exception guards |
| Memory growth during long missions | Performance | Bounded ring buffers for telemetry history; aggressive log rotation |
| MAVLink dialect mismatch | Integration | Runtime dialect detection; strict validation of incoming message schemas |

---

## 10. Appendix: Technology Justifications (ADRs)

See `docs/adrs/` for detailed decisions:
- **ADR-001:** PySide6 over PyQt5/6 (Licensing, Qt6 features)
- **ADR-002:** Topic-based Bus over ROS2 DDS (Simplicity, Python ecosystem)
- **ADR-003:** CesiumJS over QGIS/Qt3D (WebGL performance, aerospace standard)
- **ADR-004:** Plugin ABCs over Qt Plugin System (Python ergonomics, hot-reload)

---

## 11. Open Questions

1. Should multi-vehicle support be in v1.0 (swarm GCS) or v1.1?
2. Should the map engine fallback to Qt3D for fully offline environments?
3. Video stream integration (RTSP/H.265) — separate plugin or core capability?
