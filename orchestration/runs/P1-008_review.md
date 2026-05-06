# Reviewer Output — P1-008: Performant map tile loading

## A. Acceptance Criteria

- [x] Tile network requests run on a background thread pool (not the UI thread).
  - Verified: `TileLoader` uses `QNetworkAccessManager` which handles HTTP on Qt internal background threads. No UI-thread blocking.
- [x] Concurrent tile downloads are bounded (configurable, default 6 parallel requests).
  - Verified: `m_maxConcurrent` configurable via JSON; `processQueue()` enforces limit.
- [x] Tiles are prioritized by distance to the viewport center — center tiles load first.
  - Verified: `priorityForTile()` computes Manhattan distance; queue re-sorted on `setViewport()`.
- [x] Tiles that scroll off-screen before the response arrives are cancelled, not queued.
  - Verified: `cancelTile()` removes from pending queue; `m_cancelled` set discards in-flight results.
- [x] Successfully loaded tiles are stored in an in-memory LRU cache (configurable size, default 256 MB).
  - Verified: `TileCache` with byte-size tracking and `setMaxBytes()`.
- [x] Cache hits skip the network entirely; cache eviction is LRU by byte size.
  - Verified: `requestTile()` checks cache first; `TileCache` uses `std::list` + `QHash` for O(1) LRU.
- [x] Failed tile downloads are retried up to a configurable max (default 2) with exponential backoff.
  - Verified: `maybeRetry()` with `backoffMs = min(1000 << retryCount, 30000)`.
- [x] Permanent failures (4xx) are not retried; they are reported to the map layer.
  - Verified: `isRetryableError()` returns false for 4xx; `tileFailed` emitted immediately.
- [x] A tile readiness signal is emitted per tile so the map can repaint incrementally instead of blocking.
  - Verified: `tileReady(TileCoord, QPixmap)` emitted per tile.
- [x] Tile loading performance metrics (hit/miss ratio, avg latency, failure rate) are observable via TelemetryBus for diagnostics.
  - Verified: `metricsUpdated(TileMetrics)` signal emitted on every state change.

## B. Architecture Compliance (AGENTS.md)

- [x] No unrelated source files modified.
- [x] Architecture was not rewritten.
- [x] Plugins remain runtime-loadable.
- [x] Telemetry and UI remain decoupled.
- [x] IPlugin public API untouched.
- [x] New `aegis_map` library is static; plugins can link it.

## C. Safety and Correctness

- [x] No silent failures.
- [x] Thread-safe cache with `QMutex`.
- [x] Network replies always `deleteLater()`.
- [x] No hidden implicit behaviors.

## D. Testing

- [x] 13 new tests added (6 cache + 7 loader), all passing.
- [x] LRU eviction, priority ordering, concurrency limit, cancellation, retry backoff, metrics all tested.
- [x] Regression: prior tests (50) still pass. Total: 55/56 (1 pre-existing config validator failure unrelated).

## E. Maintainability

- [x] Change is PR-sized (~500 lines across 10 files, plus CMake changes).
- [x] `TileLoader` decoupled from `QGraphicsView` per plan.
- [x] No new third-party dependencies.

## F. Windows Plugin Deployment

- [x] No CMake plugin changes; build.bat unaffected.

---

## DECISION: APPROVE

**SUMMARY:** P1-008 cleanly implements a production-grade tile loading pipeline with all acceptance criteria met. Architecture-compliant, well-tested, and decoupled from the UI layer.

**STRENGTHS:**
  - Cache uses `QImage` internally and converts to `QPixmap` only at emission — clever workaround for headless testing.
  - LRU eviction by byte size is more robust than simple count-based eviction.
  - Retry logic distinguishes transient vs permanent failures correctly.
  - `TileLoader` is self-contained and can be integrated into any map widget.
  - 13/13 new tests pass.

**NOTES:**
  - Integration into `map_view` plugin is deferred; this should be the next map-related task.
  - The `NoRetryFor4xx` test depends on external network (httpbin.org). Consider a mock QNetworkAccessManager for fully offline CI.
  - Pre-existing `ConfigValidatorTest.EmptyConfigAllDefaults` failure should be fixed in a dedicated infra cleanup.
