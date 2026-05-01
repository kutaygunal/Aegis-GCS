# AEGIS GCS

**A**erospace **E**xtensible **G**round **C**ontrol **S**tation — A modular, plugin-based operator platform for UAS telemetry, mission planning, and real-time visualization.

> Built as a portfolio project demonstrating enterprise-grade application architecture for mission-critical aerospace software.

---

## Overview

AEGIS GCS is a **Qt-based** (PySide6) desktop ground control station that ingests live MAVLink telemetry and exposes it to a dynamic, dockable UI workspace built on Qt's advanced docking and graphics frameworks. The core differentiator is its **runtime plugin architecture** — new capability panels (telemetry HUD, mapping, mission planning, alerting) can be loaded without recompiling the shell.

This project targets the competencies expected of a senior Aerospace Application Software Engineer:
- Modular, maintainable architecture across multiple subsystem interfaces
- Real-time telemetry ingestion and multi-threaded data flow
- Operator-centric UI/UX with configurable workflows
- Geospatial visualization and mission planning
- CI/CD, automated testing, and strict interface contracts

---

## Architecture at a Glance

```
┌─────────────────────────────────────────────┐
│           AEGIS Shell (PySide6)             │
│  ┌─────────┐ ┌─────────┐ ┌───────────────┐  │
│  │  HUD    │ │   Map   │ │ Mission Editor│  │  ← Plugin Workspace
│  │ Plugin  │ │ Plugin  │ │   Plugin      │  │    (dockable, hot-swappable)
│  └─────────┘ └─────────┘ └───────────────┘  │
├─────────────────────────────────────────────┤
│         Plugin SDK (Interface Contracts)      │
├─────────────────────────────────────────────┤
│        Core Services (Bus / State / IO)       │
│  • Telemetry Bus (thread-safe pub/sub)        │
│  • Vehicle State Model                      │
│  • MAVLink IO Manager                       │
│  • Plugin Loader / Lifecycle Manager          │
└─────────────────────────────────────────────┘
                       │
              ┌────────┴────────┐
              │  MAVLink (UDP)  │
              └─────────────────┘
                       │
          ┌────────────┴────────────┐
      ┌───┴───┐                ┌────┴────┐
   PX4 SITL   │                │  .tlog  │
   (Gazebo)   │                │ Replay  │
              └─────────────────────────┘
```

---

## Tech Stack

| Layer | Technology |
|-------|-----------|
| Shell & UI | Python 3.11 + PySide6 (Qt 6) |
| Plugin SDK | Abstract Base Classes + runtime importlib |
| Telemetry | MAVLink 2 (pymavlink) |
| Mapping | Qt Location + Qt 3D / CesiumJS via Qt WebEngine |
| State Bus | Qt Signals & Slots (Qt::QueuedConnection cross-thread) |
| Storage | SQLite (log replay) |
| Testing | pytest + pytest-qt + coverage |
| CI/CD | GitHub Actions |

---

## Repository Structure

```
aegis-gcs/
├── .github/workflows/     # CI/CD definitions
├── config/                # MAVLink dialects, UI themes, keybindings
├── docs/                  # Architecture Decision Records (ADRs), design docs
│   ├── architecture.md
│   └── adrs/
├── scripts/               # Dev helpers (bootstrap, SITL launch)
│   └── start_sitl.sh / .bat
├── src/aegis/
│   ├── core/
│   │   ├── __init__.py
│   │   ├── bus.py           # TelemetryBus: thread-safe pub/sub
│   │   ├── state.py         # VehicleState: unified data model
│   │   ├── plugin_host.py   # PluginLoader + lifecycle
│   │   └── interfaces.py    # IPlugin, ITelemetrySink, etc.
│   ├── ui/
│   │   ├── main_window.py   # Dockable workspace shell
│   │   ├── dock_manager.py  # Drag/drop panel layout
│   │   └── theme.py
│   ├── telemetry/
│   │   ├── mavlink_io.py    # UDP/TCP MAVLink connection
│   │   ├── parsers.py       # Message demux → state updates
│   │   └── replay.py        # .tlog file reader
│   ├── mapping/
│   │   └── cesium_view.py   # Map viewport wrapper
│   ├── plugins/
│   │   ├── __init__.py
│   │   ├── telemetry_hud/   # Example plugin
│   │   ├── mission_editor/  # Example plugin
│   │   └── alert_console/   # Example plugin
│   └── utils/
│       └── logging_config.py
├── tests/
│   ├── unit/              # Core service tests
│   └── integration/       # End-to-end telemetry → UI tests
├── requirements.txt
├── requirements-dev.txt
├── pyproject.toml
└── README.md
```

---

## Quick Start

### Prerequisites
- Python 3.11+
- pip

### Install
```bash
git clone https://github.com/yourusername/aegis-gcs.git
cd aegis-gcs
python -m venv .venv
source .venv/bin/activate  # Windows: .venv\Scripts\activate
pip install -r requirements-dev.txt
```

### Run with Simulated Telemetry
```bash
# Terminal 1 — Start PX4 SITL (requires Docker or PX4 toolchain)
./scripts/start_sitl.sh

# Terminal 2 — Launch AEGIS
python -m aegis
```

### Run with Log Replay
```bash
python -m aegis --replay sample_mission.tlog
```

---

## Plugin Development

Plugins are self-contained packages that implement `IPlugin`.

```python
# src/aegis/plugins/my_plugin/__init__.py
from aegis.core.interfaces import IPlugin
from PySide6.QtWidgets import QWidget

class MyPlugin(IPlugin):
    name = "My Plugin"
    version = "1.0.0"

    def initialize(self, bus, state):
        self._widget = QWidget()
        # ... build UI
        bus.subscribe("HEARTBEAT", self._on_heartbeat)

    def widget(self) -> QWidget:
        return self._widget

    def shutdown(self):
        pass
```

Drop the package into `src/aegis/plugins/` — AEGIS discovers it at runtime.

---

## Testing

```bash
# Unit tests
pytest tests/unit -v --cov=src/aegis --cov-report=html

# Integration tests (requires SITL running)
pytest tests/integration -v
```

---

## Roadmap

- [x] Repo scaffolding & architecture doc
- [ ] Core telemetry bus + MAVLink I/O
- [ ] Plugin SDK v1 (load, lifecycle, contracts)
- [ ] Shell workspace with docking
- [ ] Telemetry HUD plugin
- [ ] Cesium mapping plugin
- [ ] Mission editor plugin (waypoints, upload)
- [ ] .tlog replay engine
- [ ] CI/CD with GitHub Actions
- [ ] ADR-001 through ADR-004

---

## License

MIT — Portfolio project.
