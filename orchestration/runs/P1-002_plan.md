# Worker Plan — P1-002: Structured rotating logs

## Objective

Extend the existing `Logger` utility in `src/utils/` to support file rotation by size and count, add thread ID to the log format, and wire new config keys (`logging.maxFiles`, `logging.maxSizeBytes`) through `Application`.

## Files to Touch

| File | Action | Rationale |
|---|---|---|
| `src/utils/logging.hpp` | Modify | Add `setRotation(qint64,int)` and rotation state fields |
| `src/utils/logging.cpp` | Modify | Implement `maybeRotate()` and `rotateFiles()`; update format with thread ID |
| `src/app/application.cpp` | Modify | Read `maxFiles`/`maxSizeBytes` from config, call `setRotation` |
| `config/aegis.json` | Modify | Add new defaults: `maxFiles: 5`, `maxSizeBytes: 10485760` |
| `tests/CMakeLists.txt` | Modify | Add `test_logger` target |
| `tests/utils/test_logger.cpp` | Create | Unit tests: file write, format, rotation boundary, max count, thread safety, level filtering |

## Acceptance Criteria Mapping

1. **Rotating files** — `setRotation` + `maybeRotate()` close current file, shift backups, reopen atomically under mutex.
2. **Format includes thread ID** — `QThread::currentThreadId()` added as hex token.
3. **Trigger-safe** — Entire write + flush + rotation decision happens inside `QMutexLocker` scope.
4. **API compatibility** — `log()`, `setLogFile()`, `setMinLevel()` signatures unchanged; `setRotation` is additive.
5. **Config honored** — `Application::initialize()` reads config and passes values to logger.

## Test Plan

- `WritesToFile` — basic file sink works.
- `FormatIncludesThreadId` — regex validates `[timestamp] [threadId] {level} component: message`.
- `RotationCreatesBackup` — small max size triggers at least one backup.
- `RotationMaxCountRespected` — `maxFiles=2` means no `.2` backup exists.
- `ThreadSafetyStress` — 4 threads x 100 logs without rotation to verify no crash and large total size.
- `ThreadSafetyWithRotation` — 4 threads x 100 logs with rotation to verify file count cap and no crash.
- `LogLevelFiltering` — `setMinLevel` correctly suppresses lower levels.

## Risks

- Pre-existing test failures (`test_ring_buffer`, `test_telemetry_bus`) are out of scope; they existed before this task.
- Rotation under heavy concurrent load is tested; Windows file rename may fail if antivirus holds handle, but `QFile::remove`/`rename` retry is handled by Qt.
