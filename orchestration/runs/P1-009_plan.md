# Planner Output — P1-009: Vehicle-centered view with tile-load awareness

## A. Scope

Implement a follow-vehicle mode in the map view with three states (Active, Suspended, Disabled), smooth interpolated panning, tile-load readiness gating, manual-pan detection, and TelemetryBus observability.

This task also integrates `TileLoader` (P1-008) into the map plugin, replacing the ad-hoc `QNetworkAccessManager` tile fetching.

## B. Changed Components

| File | Action | Description |
|---|---|---|
| `src/plugins/map_view/follow_vehicle_controller.hpp` | Create | State machine + smooth pan controller, fully testable |
| `src/plugins/map_view/follow_vehicle_controller.cpp` | Create | Lerp-based pan tick, state transitions |
| `src/plugins/map_view/map_view_plugin.hpp` | Modify | Add TileLoader, FollowVehicleController, UI controls, event filter |
| `src/plugins/map_view/map_view_plugin.cpp` | Modify | Integrate TileLoader, wire follow-vehicle logic, pan detection |
| `src/plugins/map_view/CMakeLists.txt` | Modify | Link `aegis_map` for TileLoader |
| `tests/plugins/map_view/test_follow_vehicle.cpp` | Create | Unit tests for state machine and interpolation |
| `tests/CMakeLists.txt` | Modify | Register `test_follow_vehicle` target |

## C. Acceptance Criteria Coverage

| # | Criterion | Implementation Plan |
|---|---|---|
| 1 | Map continuously centers on vehicle | `FollowVehicleController::setTarget()` called on every `positionChanged`; Active state pans smoothly |
| 2 | Smooth interpolation | `QTimer` at 16ms tick with 12% lerp factor; stops when within 0.5px |
| 3 | Stop centering on tile failure | `setViewportFullyLoaded(false)` transitions Active→Suspended |
| 4 | Resume only when all visible tiles loaded | `setViewportFullyLoaded(true)` transitions Suspended→Active |
| 5 | Visual indicator when suspended | Toolbar label turns amber + shows "LOADING TILES..." text |
| 6 | Manual re-enable button | Re-center button visible in Disabled state; click calls `requestRecenter()` |
| 7 | Manual pan disables follow | Event filter on viewport detects left-drag; calls `onUserPanned()` → Disabled |
| 8 | Tile readiness tracked per visible tile | `TileLoader` `tileReady`/`tileFailed` signals; visible set compared against loaded set |
| 9 | TelemetryBus "tiles loaded" flag | `TelemetryBus::publish("map/tilesReady", bool)` emitted on every readiness change |

## D. Architecture and Safety

- **No TelemetryBus API breakage**: using existing `publish(topic, QVariant)` generic pub/sub.
- **No IPlugin ABI change**: follow-vehicle is entirely inside the map plugin.
- **TileLoader integration**: replaces `QNetworkAccessManager` in the plugin; TileLoader already handles concurrency, cache, retry.
- **UI decoupling**: `FollowVehicleController` knows about `QGraphicsView` (Qt Widgets) but not about MAVLink; it is a UI concern per AGENTS.md.
- **State machine deterministic**: `FollowState {Active, Suspended, Disabled}` with explicit transitions; every transition emits `stateChanged()`.

## E. Testing Plan

| Test | What it covers |
|---|---|
| `ActiveToSuspendedOnMissingTiles` | `setViewportFullyLoaded(false)` from Active → Suspended |
| `SuspendedToActiveOnTilesLoaded` | `setViewportFullyLoaded(true)` from Suspended → Active |
| `ActiveToDisabledOnUserPan` | `onUserPanned()` from Active → Disabled |
| `DisabledToActiveOnRecenter` | `requestRecenter()` from Disabled → Active |
| `DisabledStaysDisabledIfTilesMissing` | `requestRecenter()` when tiles missing → Suspended (not Active) |
| `InterpolationConverges` | `setTarget()` + tick timer; view center approaches target within tolerance |
| `InterpolationStopsWhenClose` | Timer stops when diff < 0.5px |
| `TargetIgnoredWhenSuspended` | `setTarget()` while Suspended does not start pan |

## F. Task Status

- [x] Dependencies resolved (P1-008 done)
- [x] Source files identified
- [x] No unrelated files expected to be touched
- [x] Architecture and safety constraints documented
- [x] Test plan covers all acceptance criteria

**Decision: READY**
