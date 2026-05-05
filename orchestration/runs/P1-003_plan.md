# Planner Output — P1-003: Diagnostic Bundle Export

## 1. Summary

Add a `DiagnosticExporter` service that collects recent log files, the runtime config JSON, and a generated manifest into a ZIP bundle. Export is callable directly or triggered via TelemetryBus signal. Execution is non-blocking (background thread via QtConcurrent). Error cases (missing files, write failures) are handled gracefully.

## 2. Scope Boundary

**In scope:**
- Minimal inline `SimpleZipWriter` (stored method, no external dependency)
- `DiagnosticExporter` class with sync and async export API
- Manifest generation with timestamp, git revision, file list
- TelemetryBus signal for UI-triggered export
- Application wiring: config reads `diagnostics.exportPath`, bus signal connected
- Unit tests for ZIP writer, exporter, error paths

**Out of scope:**
- UI menu item or toolbar button (plugins can trigger via bus)
- Compression (stored method only; fastest, sufficient for text logs)
- Encryption or password protection of the bundle
- Automatic periodic export / scheduled dumps

**Architecture touch:** NO — adds a new utility class and a new TelemetryBus signal. Does not modify IPlugin, VehicleState, or existing public APIs.

## 3. Affected Files

| File | Change | Why |
|---|---|---|
| `src/utils/simple_zip_writer.hpp` | Create | Minimal ZIP writer (stored method) |
| `src/utils/simple_zip_writer.cpp` | Create | CRC32 + ZIP format implementation |
| `src/utils/diagnostic_exporter.hpp` | Create | Bundle export service interface |
| `src/utils/diagnostic_exporter.cpp` | Create | Collect files, write ZIP, manifest, async wrapper |
| `src/core/bus/telemetry_bus.hpp` | Modify | Add `emitDiagnosticExportRequested` signal |
| `src/core/bus/telemetry_bus.cpp` | Modify | Implement emitter |
| `src/utils/CMakeLists.txt` | Modify | Add new sources |
| `src/app/application.cpp` | Modify | Create exporter, wire bus signal, read diagnostics config |
| `src/app/application.hpp` | Modify | Add `DiagnosticExporter` member |
| `config/aegis.json` | Modify | Add `diagnostics` section with `exportPath` |
| `tests/CMakeLists.txt` | Modify | Add `test_diagnostic_exporter` target |
| `tests/utils/test_diagnostic_exporter.cpp` | Create | Unit + integration tests |

## 4. New Files

### `src/utils/simple_zip_writer.hpp/cpp`
- Writes ZIP with stored (uncompressed) entries
- ~150 lines total, no external deps beyond Qt file I/O

### `src/utils/diagnostic_exporter.hpp/cpp`
- Collects log files from rotation set, config JSON, generates manifest
- Sync: `exportBundle(path, error)`
- Async: `exportBundleAsync(path)` with `exportFinished` signal

## 5. Interface Design

**TelemetryBus additions:**
```cpp
void emitDiagnosticExportRequested(const QString& path);
signals: void diagnosticExportRequested(const QString& path);
```

**DiagnosticExporter:**
```cpp
class DiagnosticExporter : public QObject {
    Q_OBJECT
public:
    DiagnosticExporter(const QString& logFilePath, int logMaxFiles,
                        const QString& configPath, QObject* parent = nullptr);
    bool exportBundle(const QString& outputPath, QString* error = nullptr);
    void exportBundleAsync(const QString& outputPath);
public slots:
    void onExportRequested(const QString& outputPath);
signals:
    void exportFinished(bool success, const QString& path, const QString& error);
};
```

## 6. State / Data Flow

```
[UI or plugin] bus->emitDiagnosticExportRequested("/path/to/bundle.zip")
  -> DiagnosticExporter::onExportRequested()
    -> QtConcurrent::run([=] {
         1. Collect files (log.0, log.1, ..., config.json)
         2. Generate manifest.json (timestamp, git rev, file list)
         3. Write ZIP via SimpleZipWriter
         4. QMetaObject::invokeMethod -> emit exportFinished()
       })
```

## 7. Risk Analysis

| Risk | Likelihood | Mitigation |
|---|---|---|
| ZIP format bugs produce unreadable bundles | Low | Tests validate ZIP by parsing EOCD and entry count |
| Async export references deleted exporter | Low | Application owns exporter; outlives all exports |
| Git not available in runtime env | Medium | Manifest shows "unknown" revision gracefully |
| Disk full during export | Low | QFile::open checks fail; error string returned |

## 8. Test Strategy

- `SimpleZipWriterTest`: Write ZIP with 2 files, verify EOCD, entry count, file names
- `DiagnosticExporterTest::SyncExport`: Export with temp files, verify ZIP exists, manifest valid
- `DiagnosticExporterTest::MissingLogFile`: Export when log file missing -> still succeeds with remaining files
- `DiagnosticExporterTest::AsyncExport`: Verify signal fires, verify result on main thread

## 9. Open Questions

None.

## 10. Dependencies Check

P1-002 (rotating logs) is `done` (commit 4b82ef9). Dependency satisfied. Proceeding.
