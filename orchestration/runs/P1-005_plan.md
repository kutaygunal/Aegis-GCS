# Planner Output — P1-005: Config Migration System

## 1. Summary

Build a `ConfigMigrator` service that transforms older config files to the current application format. The migrator maintains a registry of version-to-version transforms (v1→v2, v2→v3, etc.), runs them automatically when a config with an older `configVersion` is loaded, writes a timestamped backup before any changes, and gracefully falls back to defaults if a migration step fails. This task also bumps the app target version to v2 and adds a concrete v1→v2 migration as a proof-of-concept.

## 2. Scope Boundary

### In scope:
- Create `ConfigMigrator` class with a registry of `version → transform` functions.
- Define a concrete v1→v2 migration that adds `telemetry.reconnectIntervalMs` (default 5000) and `logging.enableConsole` (default true).
- Bump `CURRENT_CONFIG_VERSION` to 2 and update `config/aegis.json` accordingly.
- Update `ConfigValidator` schema for v2 keys.
- On config load: parse → migrate (if needed, with backup) → validate.
- Add tests for: no migration (current version), single-step (v1→v2), multi-step (v1→v2→v3), backup created, migration failure fallback, unknown key preservation.

### Out of scope:
- GUI for migration preview or user confirmation.
- Rollback UI for backups.
- Changing config file format (stays JSON).

### Architecture touch:
- **No** changes to `IPlugin`, `TelemetryBus`, or `VehicleState` public API.
- `Application::loadConfiguration()` gains a migration step. Zero ABI impact.

## 3. Affected Files (Predicted)

| File | Change | Why |
|---|---|---|
| `src/utils/config_migrator.hpp` | Create | Migration registry and runner interface |
| `src/utils/config_migrator.cpp` | Create | Migration engine, backup writer, chain runner |
| `src/utils/config_validator.hpp/cpp` | Modify | Update schema for v2 keys (reconnectIntervalMs, enableConsole) |
| `src/utils/CMakeLists.txt` | Modify | Add new sources |
| `src/app/application.cpp` | Modify | Insert migration step between JSON parse and validation |
| `config/aegis.json` | Modify | Bump to `"configVersion": 2`, add new keys |
| `tests/utils/test_config_migrator.cpp` | Create | Unit + integration tests |
| `tests/CMakeLists.txt` | Modify | Add `test_config_migrator` target |
| `orchestration/tasks.yaml` | Modify | Mark P1-005 done after review |

## 4. New Files

### `src/utils/config_migrator.hpp`
- **Responsibility:** Registry and runner for versioned config migrations.
- **Key interface:**
  ```cpp
  using MigrationFunc = std::function<QJsonObject(const QJsonObject&)>;

  class ConfigMigrator {
  public:
      void registerMigration(int fromVersion, MigrationFunc func);
      QJsonObject migrate(const QJsonObject& config, int targetVersion,
                          QString* error = nullptr) const;
      static bool writeBackup(const QJsonObject& original,
                              const QString& basePath, QString* backupPath = nullptr);
      static ConfigMigrator createAegisMigrator();
      static constexpr int CurrentVersion = 2;
  private:
      QMap<int, MigrationFunc> m_registry; // fromVersion → func
  };
  ```

### `src/utils/config_migrator.cpp`
- **Responsibility:** Chain migration logic (v1→v2→v3), backup writer, AEGIS migration definitions.

### `tests/utils/test_config_migrator.cpp`
- **Responsibility:** ~10 tests covering all migration paths.

## 5. Interface Design

### ConfigMigrator
- `registerMigration(int fromVersion, func)` adds a transform. The transform receives a QJsonObject and returns a new QJsonObject.
- `migrate(config, targetVersion)` walks the registry from `configVersion` up to `targetVersion - 1`, applying each transform in order.
- If any step is missing from the registry, migration fails with an error string.
- If any transform throws or returns an invalid object, migration fails.
- `writeBackup()` writes the original JSON to `<basePath>.backup.<ISO8601>.json`.

### Application changes
- `Application::loadConfiguration()` new flow:
  1. Read JSON file.
  2. Detect `configVersion` (default to 1 if missing).
  3. If version < `ConfigMigrator::CurrentVersion`:
     a. Write backup.
     b. Run `ConfigMigrator::migrate()`.
     c. Log migration result.
  4. Convert to `QVariantMap` for validator.

### v1→v2 migration transform
- Adds `telemetry.reconnectIntervalMs = 5000` if missing.
- Adds `logging.enableConsole = true` if missing.
- Bumps `configVersion` from 1 to 2.
- Preserves all other keys (including unknown keys).

### ConfigValidator schema updates
- Add `telemetry.reconnectIntervalMs` as optional int (default 5000, min 100).
- Add `logging.enableConsole` as optional bool (default true).
- Update `configVersion` default to 2.

### Config changes
- `config/aegis.json` updated to `configVersion: 2` with new keys.

## 6. State / Data Flow

### Startup path
1. `Application::loadConfiguration()` reads JSON.
2. Extract `configVersion` (default 1 if missing).
3. If version < 2:
   - Call `ConfigMigrator::writeBackup()` → writes `aegis.json.backup.20260105T143022.json`.
   - Call `migrator.migrate(json, 2)` → returns transformed JSON.
   - Log: `[Config] Migrated v1 → v2, backup at <path>`.
4. Convert JSON object to `QVariantMap`.
5. Pass to `ConfigValidator::validate()`.

### Error paths
- Missing migration step → log error, fall back to default config.
- Backup write failure → log warning, continue with migration (best effort).
- Transform returns invalid JSON → log error, fall back to defaults.

## 7. Risk Analysis

| Risk | Likelihood | Mitigation | Fallback |
|---|---|---|---|
| Migration corrupts user config | Low | Backup written first; transforms are pure and additive | User can restore backup manually |
| Chained migration has missing step | Low | Registry validated at registration; tested with multi-step | Log error, fall back to defaults |
| Config file is v0 (no configVersion) | Medium | Treat as v1 (default if missing) | Works transparently |

## 8. Test Strategy

### Unit tests (`test_config_migrator`)

| Test | Validates | Expected count |
|---|---|---|
| `NoMigrationNeeded` | v2 config with target v2 → unchanged, no backup | 1 |
| `SingleStepMigration` | v1 config → v2 with new keys added, version bumped | 1 |
| `MultiStepMigration` | v1 config → v3 via v1→v2 and v2→v3 chain | 1 |
| `BackupCreated` | Backup file exists after migration with original content | 1 |
| `UnknownKeysPreserved` | Custom key survives v1→v2 | 1 |
| `MissingMigrationStepFails` | v1 config with target v3 but only v1→v2 registered → error | 1 |
| `MigrationFailureFallback` | Bad transform returns empty object → error | 1 |
| `V0TreatedAsV1` | Config without configVersion treated as v1 and migrated | 1 |
| `MigrationDoesNotTouchExistingValues` | Existing values in v1 config stay unchanged | 1 |

Total: ~9 tests.

### Integration test
- Build and run `aegis` with v1 config file → app starts, backup created, config migrated.
- Build and run with v2 config → no migration, normal startup.

## 9. Open Questions

None.

## 10. Dependencies Check

- P1-005 `depends_on: [P1-004]` — P1-004 is `done` (commit `8bf8156`).
- **Proceed with implementation.**
