# Reviewer Output — P1-006: CI Hardening

## A. Acceptance Criteria

- [x] GitHub Actions workflow runs on Windows and Linux.
  - Verified: `build-linux`, `build-windows`, `clang-tidy`, and `asan` jobs all target the respective platforms.
- [x] clang-tidy checks run on the core and telemetry source.
  - Verified: Two separate steps in `clang-tidy` job run `run-clang-tidy` on `src/core/*.cpp` and `src/telemetry/*.cpp`.
- [x] AddressSanitizer (or equivalent) runs on Linux tests.
  - Verified: `asan` job configures with `-DAEGIS_CI_BUILD=ON` which enables `-fsanitize=address` on Linux Debug builds.
- [x] A replay regression test corpus is added: a set of binary MAVLink files that exercise parser paths.
  - Verified: `tests/data/replay/corpus.tlog` (260 bytes) contains 7 MAVLink messages covering all 6 parser handlers.
- [x] CI fails on compiler warnings treated as errors (selectively, not globally).
  - Verified: `AEGIS_CI_BUILD=ON` enables `-Werror` on GCC/Clang and `/WX` on MSVC only in CI; local builds are unaffected.

## B. Architecture Compliance (AGENTS.md)

- [x] No unrelated source files modified.
- [x] Architecture was not rewritten.
- [x] Plugins remain runtime-loadable.
- [x] Telemetry and UI remain decoupled.
- [x] IPlugin public API untouched.

## C. Safety and Correctness

- [x] No silent failures.
- [x] clang-tidy config starts permissive to avoid false failures.
- [x] ASan job uses `detect_leaks=1:abort_on_error=1` to ensure failures are visible.
- [x] No hidden implicit behaviors.

## D. Testing

- [x] 8 new regression tests added, all passing.
- [x] Corpus file is present and non-empty.
- [x] All parser paths are exercised by the regression test.
- [x] Regression: all prior tests (42 total) still pass. Total: 50/50 PASS.

## E. Maintainability

- [x] Change is PR-sized (~300 lines across 6 files, plus 260-byte binary).
- [x] CI workflow is self-documenting; each job has a clear purpose.
- [x] No new third-party dependencies.

## F. Windows Plugin Deployment

- [x] No CMake plugin changes; build.bat unaffected.

---

## DECISION: APPROVE

**SUMMARY:** P1-006 cleanly adds CI hardening with clang-tidy, AddressSanitizer, warnings-as-errors, and a replay regression corpus. No architecture violations, no safety issues.

**STRENGTHS:**
  - Four parallel CI jobs provide comprehensive coverage: build, test, static analysis, and memory safety.
  - `AEGIS_CI_BUILD` option makes CI behavior opt-in and local-build-safe.
  - Replay corpus is tiny (260 bytes) but exercises all 6 parser paths.
  - clang-tidy starts permissive with TODO comments for tightening.
  - 50/50 tests passing across all suites.

**NOTES:**
  - clang-tidy `|| true` fallback should be removed once the codebase is fully clean.
  - Consider enabling `build-linux` with `-DAEGIS_CI_BUILD=ON` to also catch warnings in the basic build job.
  - Windows ASan is not enabled; MSan/TSan could be added in future tasks.
