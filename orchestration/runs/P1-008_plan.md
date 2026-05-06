# Planner Output — P1-008: Performant map tile loading

## 1. Task Description

Implement a performant, concurrent tile loading pipeline with LRU caching, bounded concurrency, viewport-center priority, cancellation, retry backoff, and metrics observability.

## 2. Why It Matters

Current tile loading (from P1-007) requests tiles but has no caching, no concurrency limits, no priority, and no cancellation. Panning quickly causes many stale requests and memory growth. This task adds production-grade tile management.

## 3. Changes To Make

### New Files

1. **`src/map/tile_loader.hpp`** — public interface
   - `TileLoader` class with signals: `tileReady(TileCoord, QPixmap)`, `tileFailed(TileCoord, QString)`, `metricsUpdated(TileMetrics)`.
   - Methods: `setViewport(ViewportInfo)`, `loadTile(TileCoord)`, `cancelTile(TileCoord)`, `clearCache()`.

2. **`src/map/tile_loader.cpp`** — implementation
   - `QThreadPool`-backed `TileFetchRunnable` for network requests.
   - Thread-safe `TileLRUCache` (read from UI, write from fetch threads).
   - Priority queue: tiles sorted by distance from viewport center; re-sorted on every viewport change.
   - Bounded concurrency: configurable max parallel requests (default 6).
   - Cancellation: off-screen tiles removed from pending queue; in-flight requests finish but results discarded.
   - Retry: exponential backoff for transient failures, max retries=2; 4xx skipped.
   - Metrics: hit/miss count, avg latency, failure rate → emitted via `TelemetryBus`.

3. **`src/map/tile_cache.hpp`** — `TileLRUCache` struct
   - `std::list` + `QHash` for O(1) LRU eviction by byte size.
   - `insert()`, `get()`, `evict()`, `byteSize()`, `hitRate()`.
   - `QMutex` for thread safety.

4. **`src/map/tile_types.hpp`** — shared tile types
   - `TileCoord { zoom, x, y }` with `qHash` support.
   - `ViewportInfo { centerLat, centerLon, zoom, widthPx, heightPx }`.
   - `TileMetrics { hits, misses, failures, avgLatencyMs, pendingCount }`.

5. **`tests/map/test_tile_loader.cpp`** — unit tests
   - Cache hit/miss/eviction, LRU ordering.
   - Priority ordering by viewport center.
   - Concurrency limit enforcement.
   - Cancellation removes stale tiles.
   - Retry backoff for 5xx; no retry for 4xx.
   - Metrics accumulation.

6. **`tests/map/test_tile_cache.cpp`** — LRU cache unit tests
   - Insert/get, eviction at capacity, byte-size tracking, hit rate.

### Modified Files

7. **`config/aegis.json`** — add `map` domain with tile cache and concurrency knobs.

8. **`src/app/application.cpp`** — instantiate `TileLoader` in `initialize()`, wire config values.

9. **`src/map/CMakeLists.txt`** — add new sources to `aegis_map` library.

10. **`src/ui/widgets/map_view.hpp/cpp`** (if exists) — integrate `TileLoader` signals into map repaint.
    - If no `map_view` widget yet, skip; `TileLoader` is self-contained.

### Files NOT to touch
- `IPlugin.hpp`, any plugin source.
- `TelemetryBus` internals beyond `emit metricsUpdated`.
- `MavlinkIO`, `VehicleState`, `CommandBus`.

## 4. Risk Assessment

- **Medium** — threading, network, and cache correctness are easy to get wrong. LRU cache must be thread-safe. Network cancellation must not crash. Metrics emission must be thread-safe.

## 5. Test Plan

### Unit Tests (`test_tile_cache`)

| Test | Input | Expected Output |
|---|---|---|
| `InsertAndGet` | Insert 3 tiles, get middle | Returns correct pixmap, hit=1 |
| `EvictionAtCapacity` | Insert 4 tiles with capacity 3 | Oldest evicted, size=3 |
| `LRUOrderAfterAccess` | Insert A,B,C; get A; insert D | B evicted (LRU), not A |
| `ByteSizeTracking` | Insert tiles with varying sizes | `byteSize()` equals sum |
| `HitRate` | 3 hits, 2 misses | `hitRate() == 0.6` |

### Unit Tests (`test_tile_loader`)

| Test | Input | Expected Output |
|---|---|---|
| `CacheHitSkipsNetwork` | load same tile twice | Second call emits `tileReady` immediately, no network |
| `PriorityCenterFirst` | Viewport center at tile(5,5); queue tiles at distance 0,1,2 | Distance-0 tile fetched first |
| `ConcurrencyLimit` | Queue 10 tiles, limit=3 | Only 3 active at once |
| `CancelOffscreenTile` | Request tile, then cancel | `tileReady` never emitted for cancelled tile |
| `RetryTransientFailure` | 5xx response, maxRetries=2 | Retried 2 more times with backoff |
| `NoRetryFor4xx` | 404 response | Fails immediately, no retry |
| `MetricsAccumulate` | Mix of hits, misses, failures | Metrics match counts |

### Build and Regression
- `cmake --build build --config Release --target aegis` → success.
- All prior tests pass.

## 6. Implementation Hints for Worker

- Use `QThreadPool::globalInstance()` with `setMaxThreadCount(6)` for bounded concurrency.
- Use `QNetworkAccessManager` from a dedicated worker thread (not the UI thread). Qt requires one `QNetworkAccessManager` per thread.
- For cancellation: keep a `QSet<TileCoord>` of active requests; when viewport changes, diff against new visible set and cancel removed ones. In-flight requests finish but check the active set before emitting.
- For priority: maintain `QList<TileFetchTask>` sorted by Manhattan distance to viewport center. Re-sort on every `setViewport()` call.
- For LRU cache: `std::list` for order + `QHash<TileCoord, iterator>` for O(1) lookup. Store `QPixmap` (or `QImage` for lighter memory). Track total bytes.
- For retry: store retry count in the task struct. Use `QTimer::singleShot(backoffMs)` on the worker thread's event loop.
- For metrics: atomic counters (`std::atomic<int>`) updated from fetch threads; snapshot emitted to main thread via queued signal.

## 7. What "Done" Looks Like

- `TileLoader` exists with full caching, priority, concurrency, cancellation, retry, and metrics.
- `test_tile_cache` has 5 passing tests.
- `test_tile_loader` has 7 passing tests.
- `config/aegis.json` has `map` domain with `tileCacheSizeMB`, `maxConcurrentDownloads`, `maxRetries`.
- CI build passes. All prior tests pass.
- `tasks.yaml` updated to `status: done`.
