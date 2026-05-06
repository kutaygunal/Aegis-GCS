# Planner Output — P1-007: Dynamic map tile loading for off-center viewport

## 1. Summary
The map plugin currently loads tiles once during initialization around a fixed center point (`m_centerLat/m_centerLon`) and never requests new tiles as the viewport changes. When the vehicle moves (via `fitViewToVehicle()`) or the user pans, the newly visible area renders as black because the corresponding OSM tiles were never fetched. This task replaces the static tile-radius logic with a viewport-driven tile loader that computes which tiles intersect the current `QGraphicsView` viewport, requests missing ones asynchronously, and removes off-screen tiles to prevent memory growth.

## 2. Scope Boundary

**In scope:**
- Modify `MapViewPlugin` to compute visible tile range from the current viewport.
- Add `updateVisibleTiles()` that requests missing tiles and prunes off-screen ones.
- Trigger tile updates after every viewport change (vehicle movement → `fitViewToVehicle()`).
- Reposition existing tiles correctly when the viewport shifts.
- Extract pure tile-math helpers for testability.
- Add a lightweight unit test for tile-visibility math without network.

**Out of scope:**
- Tile disk caching (P4-001).
- Offline/MBTiles support (P4-002).
- Multiple map providers (P4-003).
- Changing the map projection or tile size.
- New UI controls for panning/zooming beyond current +/- buttons.

**Architecture touch:**
- Does NOT touch `IPlugin`, `TelemetryBus`, or `VehicleState` public API.
- Low risk; changes are confined to the `map_view` plugin.

## 3. Affected Files (Predicted)

| File | Change | Why |
|------|--------|-----|
| `src/plugins/map_view/map_view_plugin.hpp` | Modify | Add `updateVisibleTiles()`, `removeOffscreenTiles()`, `visibleSceneRect()`, and `mapMath` helper includes. |
| `src/plugins/map_view/map_view_plugin.cpp` | Modify | Implement viewport-based tile loading; hook into `fitViewToVehicle()` and `onPositionChanged()`. |
| `src/plugins/map_view/map_math.hpp` | Create | Extract pure Web-Mercator tile-math functions for unit testing. |
| `tests/plugins/test_map_math.cpp` | Create | Unit tests for `worldPixelToTile`, `visibleTileRange`, and `latLonToWorldPixel` equivalence. |
| `src/plugins/map_view/CMakeLists.txt` | Modify | Add test target and any new source dependencies. |

## 4. New Files

### `src/plugins/map_view/map_math.hpp`
A header-only namespace `aegis::plugins::map_math` containing:
- `QPoint worldPixelToTile(qreal wx, qreal wy, int tileSize)`
- `QRect visibleTileRange(const QRectF& sceneRect, const QPointF& centerWorldPixel, int zoom, int tileSize)`
- `QPointF latLonToWorldPixel(qreal lat, qreal lon, int zoom, int tileSize)`

All functions are pure math; no Qt GUI or network dependencies.

### `tests/plugins/test_map_math.cpp`
Unit tests for the math functions above, using GoogleTest. Verifies:
- Known lat/lon produces expected tile at zoom 15
- Viewport-to-tile range covers expected tiles
- Clamping at zoom bounds

## 5. Interface Design

No new public plugin API. Internal plugin changes only:

**MapViewPlugin private methods added:**
```cpp
void updateVisibleTiles();                          // viewport-driven load + prune
void removeOffscreenTiles(const QRectF& viewport);  // delete tiles outside margin
QRectF visibleSceneRect() const;                    // viewport mapped to scene coords
```

**MapViewPlugin behavior changes:**
- `onPositionChanged()` now calls `updateVisibleTiles()` after `fitViewToVehicle()`.
- `setupScene()` still calls `updateTiles()` for initial load (to be renamed or kept).
- `zoomIn()` / `zoomOut()` behavior unchanged (clear + rebuild).

## 6. State / Data Flow

**Use case: Vehicle moves to new area**
1. `MavlinkIO` emits `positionChanged` → `TelemetryBus` → `MapViewPlugin::onPositionChanged()`
2. Plugin updates `m_lastLat/m_lastLon`, track history, and vehicle marker position.
3. `fitViewToVehicle()` scrolls `QGraphicsView` so the vehicle stays centered.
4. `updateVisibleTiles()`:
   a. Maps viewport rect to scene coordinates.
   b. Converts scene corners to world pixel coordinates using `m_centerLat/m_centerLon` anchor.
   c. Computes tile X/Y range via `map_math` helpers.
   d. For each tile in range: calls `requestTile()` (deduplicates via `m_tileItems`).
   e. Calls `removeOffscreenTiles()` to delete tiles outside a 2-tile margin.
5. `requestTile()` either repositions an existing tile or enqueues a new `QNetworkReply`.
6. `onTileDownloaded()` paints the pixmap when the reply finishes.

**Error path:**
- Tile download failure is logged (existing behavior) and placeholder remains.
- If the viewport changes again before a pending reply finishes, the reply is still handled; the tile is added if still in `m_tileItems`.

## 7. Risk Analysis

| Risk | Likelihood | Mitigation | Fallback |
|------|------------|------------|----------|
| Tile flickering during rapid viewport changes | Medium | 2-tile margin on removal prevents thrashing; dedup prevents duplicate requests | If observed, increase margin or add a short timer-based debounce |
| Scene coordinate overflow for far-away vehicle | Low | `QGraphicsScene` supports arbitrary coordinates; we only load visible tiles | If performance degrades, clamp `m_centerLat/m_centerLon` to first fix and never update |
| Breaking zoom in/out | Low | Zoom functions clear the scene and rebuild; we preserve this | Revert to original `setupScene()` + `updateTiles()` logic for zoom |
| QNetworkAccessManager queue overflow | Low | Existing dedup via `m_tileItems` prevents duplicate requests; removal of off-screen tiles caps total items | Add max-in-flight limit if needed (follow-up task) |

## 8. Test Strategy

**Unit tests (`test_map_math`):**
- `LatLonToTile_SanFrancisco_Zoom15` — known lat/lon → expected tile x/y.
- `VisibleTileRange_CoversViewport` — given a scene rect and center, expect correct min/max tiles.
- `VisibleTileRange_ClampedAtBounds` — at low zoom, negative or >maxTile values are clamped.

**Integration tests (manual / existing):**
- Dummy telemetry generator moves vehicle across tile boundaries; map should remain covered.
- Zoom in/out still rebuilds correctly.

**Approximate new test cases:** 3–5 in `test_map_math.cpp`.

## 9. Open Questions

None. The viewport math is deterministic and the existing `requestTile()` / `onTileDownloaded()` infrastructure handles async loading.

## 10. Dependencies Check

`depends_on: []` — all dependencies are satisfied (this is a self-contained plugin fix).

---
*Plan generated by Planner agent. Ready for Worker implementation.*
