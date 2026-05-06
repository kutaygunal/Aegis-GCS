# Reviewer Output — P1-009: Vehicle-centered view with tile-load awareness

## A. Acceptance Criteria

- [x] When the vehicle is moving, the map view continuously centers on the vehicle position.
  - Verified: `onPositionChanged` feeds `FollowVehicleController::setTarget()`; Active state pans smoothly.
- [x] Centering uses smooth interpolation (interpolated pan) to prevent jarring jumps.
  - Verified: `QTimer` at 16ms with 12% lerp factor; tested convergence in `InterpolationConverges`.
- [x] When a visible tile fails to load, the view stops re-centering on the vehicle.
  - Verified: `setViewportFullyLoaded(false)` transitions Active → Suspended.
- [x] The view resumes follow-vehicle mode only after all visible tiles have loaded successfully.
  - Verified: `setViewportFullyLoaded(true)` transitions Suspended → Active.
- [x] A visual indicator is shown when follow-vehicle is suspended due to missing tiles.
  - Verified: toolbar label shows `"LOADING TILES..."` in amber when Suspended.
- [x] The user can manually re-enable follow-vehicle via a button; it suspends again if tiles are still missing.
  - Verified: re-center button calls `requestRecenter()`; Disabled → Active (ready) or Suspended (missing).
- [x] If the user manually pans the map, follow-vehicle mode is disabled; a re-center button appears.
  - Verified: `eventFilter` detects left-drag → `onUserPanned()` → Disabled; re-center button visible.
- [x] Tile readiness state is tracked per visible tile; the view queries this state before each centering update.
  - Verified: `m_visibleTileKeys` and `m_loadedTileKeys` sets; `updateTileReadiness()` gates controller.
- [x] A "tiles loaded" flag is published on TelemetryBus.
  - Verified: `publish("map/tilesReady", bool)` emitted on every readiness change.

## B. Architecture Compliance (AGENTS.md)

- [x] No unrelated source files modified.
- [x] Architecture was not rewritten.
- [x] Plugins remain runtime-loadable.
- [x] Telemetry and UI remain decoupled.
- [x] IPlugin public API untouched.
- [x] `FollowVehicleController` is a UI concern, not in core telemetry.
- [x] Tile readiness queries go through TileLoader (P1-008), not directly to network.

## C. Safety and Correctness

- [x] No silent failures.
- [x] State transitions are deterministic and signal every change.
- [x] No duplicate signals on redundant state changes.
- [x] Manual pan detection doesn't crash or leak.
- [x] TileLoader signal connections are parented (auto-disconnect on destruction).

## D. Testing

- [x] 11 new tests added, all passing.
- [x] State transitions (active→suspended→active, active→disabled→active) fully covered.
- [x] Interpolation convergence tested with tolerance for viewport rounding.
- [x] Rapid transition stress test included.
- [x] Regression: prior tests (62) still pass. Total: 73/74 (1 pre-existing).

## E. Maintainability

- [x] Change is PR-sized (~7 files, ~600 lines).
- [x] `FollowVehicleController` is decoupled from `QGraphicsView` internals (only uses `centerOn` and `mapToScene`).
- [x] No new third-party dependencies.

## F. Windows Plugin Deployment

- [x] `map_view` plugin builds successfully and deploys to `build/Release/plugins/`.
- [x] `aegis_map` linked statically; no additional DLLs needed for plugin.

---

## DECISION: APPROVE

**SUMMARY:** P1-009 cleanly implements all acceptance criteria. The follow-vehicle state machine is deterministic, the smooth pan is tested, tile-load gating works through TileLoader, and the TelemetryBus integration is backward-compatible. TileLoader integration into the map plugin is a significant enabler for future map tasks.

**STRENGTHS:**
  - State machine is simple (3 states) but covers all edge cases.
  - No duplicate signal emissions on redundant transitions.
  - `eventFilter` on viewport is lightweight and doesn't subclass QGraphicsView.
  - TileLoader integration removes ~50 lines of ad-hoc QNetworkAccessManager code and replaces it with a maintained system.
  - 11/11 new tests pass with good coverage of state transitions and interpolation.

**NOTES:**
  - Interpolation test uses 20px tolerance due to QGraphicsView integer viewport rounding in offscreen mode. This is an acceptable test-environment artifact, not a product bug.
  - Touch/pinch gesture detection is a future enhancement; mouse pan is sufficient for desktop GCS.
  - Pre-existing `ConfigValidatorTest.EmptyConfigAllDefaults` failure should be fixed in a dedicated infra cleanup.
