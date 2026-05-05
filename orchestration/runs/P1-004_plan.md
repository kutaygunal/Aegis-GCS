# Planner Output — P1-004: Config Schema Validation

## 1. Summary

Extract the ad-hoc config normalization logic from `application.cpp` into a dedicated, declarative `ConfigValidator` class. The validator defines a schema for every top-level and nested config domain, validates types/ranges, warns on unknown keys, and provides sensible defaults for missing values. This enables forward-compatible config files, clear error messages at startup, and testable validation logic independent of the application lifecycle.

## 2. Scope Boundary

### In scope:
- Create `ConfigSchema` — a lightweight declarative schema DSL (type, required, default, range, allowed values, nested schema).
- Create `ConfigValidator` — validates a `QVariantMap` against a `ConfigSchema`, produces warnings with key path context, and returns a normalized `QVariantMap`.
- Move existing inline validation from `application.cpp` (`validateAndNormalizeConfig`, `defaultConfig`) into the new validator class.
- Wire `Application::loadConfiguration()` to use `ConfigValidator` before any service initialization.
- Add `configVersion` key to `config/aegis.json` (value 1) as the first step toward future migration support.
- Add tests for: valid config, missing required keys, wrong types, out-of-range values, unknown keys, nested plugin config errors.

### Out of scope:
- Full migration system (P1-005 handles version-to-version transforms).
- External schema library (JSON Schema, etc.).
- Changing the config file format (stays as flat JSON with nested objects).
- UI for config editing.

### Architecture touch:
- **No** changes to `IPlugin`, `TelemetryBus`, or `VehicleState` public API.
- `Application` gains a new utility dependency only. Zero ABI impact.

## 3. Affected Files (Predicted)

| File | Change | Why |
|---|---|---|
| `src/utils/config_validator.hpp` | Create | Schema definition and validator interface |
| `src/utils/config_validator.cpp` | Create | Validation engine: type checking, range checking, normalization |
| `src/utils/CMakeLists.txt` | Modify | Add new sources to aegis_utils |
| `src/app/application.cpp` | Modify | Replace inline `validateAndNormalizeConfig` / `defaultConfig` with ConfigValidator usage |
| `src/app/application.hpp` | Modify | Remove `QVariantMap m_config` default helper declarations (now in validator) |
| `config/aegis.json` | Modify | Add `"configVersion": 1` at top level |
| `tests/utils/test_config_validator.cpp` | Create | Unit tests for schema + validator |
| `tests/CMakeLists.txt` | Modify | Add test target for `test_config_validator` |
| `orchestration/tasks.yaml` | Modify | Mark P1-004 done after review |

## 4. New Files

### `src/utils/config_validator.hpp`
- **Responsibility:** Declarative schema DSL and validation entry point.
- **Key interface:**
  ```cpp
  struct SchemaRule {
      QString key;
      QString type;           // "string", "int", "bool", "double", "array", "object"
      bool required = false;
      QVariant defaultValue;
      QVariant min;           // for int/double
      QVariant max;           // for int/double
      QStringList allowed;    // for string enum
      QMap<QString, SchemaRule> nested; // for object
  };

  class ConfigValidator : public QObject {
      Q_OBJECT
  public:
      explicit ConfigValidator(QObject* parent = nullptr);
      void addTopLevelRule(const SchemaRule& rule);
      QVariantMap validate(const QVariantMap& raw, QStringList* warnings = nullptr) const;
  };
  ```

### `src/utils/config_validator.cpp`
- **Responsibility:** Implement recursive validation, type coercion, range clamping, unknown-key detection.

### `tests/utils/test_config_validator.cpp`
- **Responsibility:** 8-10 tests covering all validator paths.

## 5. Interface Design

### ConfigSchema / SchemaRule
- Schema is a flat `QList<SchemaRule>` at the top level. Each rule describes one top-level key.
- For nested objects (e.g., `telemetry`, `plugins.*`), `SchemaRule::nested` contains child rules.
- Arrays are validated element-by-element if `nested` contains an `element` rule.

### ConfigValidator::validate()
- Input: raw `QVariantMap` from JSON parse.
- Output: normalized `QVariantMap` with defaults filled in, ranges clamped, types coerced.
- Side effect: `warnings` list populated with human-readable strings like:
  - `"telemetry.bindPort: expected int, got string 'abc'; using default 14550"`
  - `"unknownKey.foo: unknown config key; ignoring"`
  - `"logging.maxSizeBytes: value -1 out of range [1, 1073741824]; clamped to 1"`

### Application changes
- `Application::loadConfiguration()` calls `ConfigValidator` after JSON parse.
- `Application::initialize()` receives the validated map directly.
- All `defaultConfig()` and `validateAndNormalizeConfig()` code in `application.cpp` is deleted (moved to validator).

### Config changes
- `config/aegis.json` gains `"configVersion": 1` at root.
- `ConfigValidator` schema marks `configVersion` as optional int with default `1`.

## 6. State / Data Flow

### Startup path
1. `Application::initialize()` calls `loadConfiguration()`.
2. `loadConfiguration()` reads JSON file → `QJsonObject` → `QVariantMap`.
3. `loadConfiguration()` creates `ConfigValidator`, adds all rules for AEGIS domains.
4. `validator.validate(rawMap, &warnings)` returns `normalizedMap`.
5. Warnings are logged via `Logger::instance().log(Warning, "Config", warning)`.
6. `normalizedMap` is stored in `m_config`.
7. Services initialize from `m_config` (unchanged behavior).

### Error paths
- Missing required key → filled with default, warning emitted.
- Wrong type → coerced if possible (e.g., string "14550" → int 14550), or defaulted with warning.
- Out of range → clamped to min/max, warning emitted.
- Unknown key → warning emitted, key preserved in output (forward compatibility).
- Invalid nested plugin config → same rules applied per-plugin.

## 7. Risk Analysis

| Risk | Likelihood | Mitigation | Fallback |
|---|---|---|---|
| Validator bug causes startup crash | Low | Unit-test every rule before integration | Keep old inline code behind `#ifdef` until validator proven |
| Type coercion surprises (e.g., bool → int) | Low | Strict type checking: reject coercion for incompatible types, use default | Log explicit warning with expected vs actual type |
| Application.cpp refactor is large | Medium | Move code line-by-line; verify behavior parity via tests | Revert to inline validation if review finds issues |
| Plugin config is free-form per plugin | Medium | Plugin config values are validated only for known plugins; unknown plugin keys are preserved | Document that plugin configs are best-effort validated |

## 8. Test Strategy

### Unit tests (`test_config_validator`)

| Test | Validates | Expected count |
|---|---|---|
| `ValidConfigAccepted` | Full valid config passes with no warnings | 1 |
| `MissingRequiredKeyUsesDefault` | Missing telemetry.bindPort gets default 14550 | 1 |
| `WrongTypeStringToInt` | String "14550" coerced to int 14550 | 1 |
| `WrongTypeUnrecoverable` | Object where int expected → default + warning | 1 |
| `OutOfRangeClamped` | logging.maxFiles = -1 → clamped to 1 | 1 |
| `UnknownKeyWarned` | Extra key "foo.bar" → warning, preserved | 1 |
| `NestedPluginConfigValid` | plugins.alert_console.maxItems = 300 → accepted | 1 |
| `NestedPluginConfigInvalid` | plugins.alert_console.maxItems = "abc" → defaulted + warning | 1 |
| `EmptyConfigAllDefaults` | Empty JSON → all defaults filled | 1 |
| `ConfigVersionOptional` | Missing configVersion → default 1, no warning | 1 |

Total: ~10 tests.

### Integration test
- Build and run `aegis` with valid config → startup succeeds.
- Build and run with malformed config → app starts with warnings, uses defaults.

## 9. Open Questions

None.

## 10. Dependencies Check

- P1-004 `depends_on: []` — no dependencies.
- All prior tasks (P1-001, P1-002, P1-003) are `done`.
- **Proceed with implementation.**
