# AEGIS GCS — Engineering Orchestration System

This directory contains the orchestration layer for AI-assisted and human-assisted development of AEGIS GCS.

## Purpose

The goal is to convert the project roadmap into small, executable, testable tasks and run them one at a time through a predictable workflow. Each task goes through: **Plan → Implement → Review → Merge**.

This prevents scope creep, preserves architecture, and keeps every change reviewable and safe.

---

## Files

| File | Purpose |
|---|---|
| `../AGENTS.md` | Engineering rules that all agents must follow. Read this first. |
| `roadmap.yaml` | High-level roadmap organized into phases. Defines goals and deliverables. |
| `tasks.yaml` | Concrete task registry. Each task has acceptance criteria, dependencies, test commands, and status. |
| `completed_tasks.yaml` | Archive of finished tasks (cut from `tasks.yaml` for readability and token savings). |
| `prompts/planner.md` | Reusable prompt for the Planner agent. Produces implementation plans. |
| `prompts/worker.md` | Reusable prompt for the Worker agent. Implements exactly one task. |
| `prompts/reviewer.md` | Reusable prompt for the Reviewer agent. Reviews diffs against acceptance criteria. |
| `runs/` | Directory for run reports. Each task execution produces a log here. |

---

## Workflow (9-Step Execution)

Every task must run through the following 9 steps in order. **Do not skip steps.** **Do not push to `main` before Step 8 (Review Approval).**

### Step 1 — Read Task YAML
Read the task definition from `tasks.yaml`. Verify:
- Status is `ready` (all `depends_on` are `done`).
- Phase is active or explicitly prioritized.
- Acceptance criteria are clear and testable.

**Artifact:** None (in-memory context).

---

### Step 2 — Create Git Branch
Create a feature branch from `main`. Never implement directly on `main`.

```bash
git checkout main
git pull origin main
git checkout -b task/P1-XXX
```

**Naming convention:** `task/<task-id>` (e.g., `task/P1-002`).

**Artifact:** Local branch `task/P1-XXX`.

---

### Step 3 — Generate Pi Prompt
Feed the Planner prompt (`prompts/planner.md`) with the task definition. The Planner must produce:
- Files to touch with rationale.
- Mapping of acceptance criteria to implementation strategy.
- Test plan with specific test names and what they verify.
- Risk and deviation notes.

**Do not let the Planner modify code.**

**Artifact:** `orchestration/runs/P1-XXX_plan.md`

---

### Step 4 — Run Pi Coding Agent
Feed the Worker prompt (`prompts/worker.md`) with:
- The task definition from `tasks.yaml`
- The Planner's plan from `runs/P1-XXX_plan.md`

The Worker must:
- Implement **only** the planned changes.
- Add or update tests for every behavior change.
- Build and run the tests listed in `tasks.yaml`.
- Produce a Run Report with build output, test output, and file inventory.

**Artifact:** `orchestration/runs/P1-XXX_run.md`

---

### Step 5 — Build Project
Compile the full project (or the minimum targets affected). Fix compilation errors before proceeding.

```bash
cmake --build build --config Release --parallel
```

**Artifact:** Build logs captured in `runs/P1-XXX_run.md`.

---

### Step 6 — Run Tests
Run the test commands listed in the task's `test_commands`. All must pass.

```bash
ctest --test-dir build --output-on-failure -C Release
```

**Artifact:** Test output captured in `runs/P1-XXX_run.md`.

---

### Step 7 — Save Logs
Commit all changes (code + tests + orchestration docs) **on the feature branch**. Do not push to `main`.

```bash
git add -A
git commit -m "P1-XXX: <task title>"
```

**Artifacts committed to branch:**
- Code changes (src/, tests/, config/)
- `orchestration/runs/P1-XXX_plan.md`
- `orchestration/runs/P1-XXX_run.md`

---

### Step 8 — Ask Review Agent to Review Diff
Feed the Reviewer prompt (`prompts/reviewer.md`) with:
- The task definition from `tasks.yaml`
- The Worker's Run Report from `runs/P1-XXX_run.md`
- The **full diff** of the feature branch against `main`

The Reviewer returns one of:
- **APPROVE** → Proceed to Step 9.
- **REQUEST_CHANGES** → Return to Step 4 with blockers. Produce `P1-XXX_run_v2.md` and `P1-XXX_review_v2.md`.

**Artifact:** `orchestration/runs/P1-XXX_review.md`

---

### Step 9 — Create Summary & Archive Task

1. Merge the approved feature branch into `main`:
```bash
git checkout main
git merge task/P1-XXX --no-ff -m "Merge P1-XXX: <task title>"
git push origin main
```

2. Update `orchestration/tasks.yaml`:
```yaml
status: done
```

3. Add a `notes` field to the task with a brief summary:
```yaml
notes: >
  Implemented ... All N tests pass. No architecture violations.
```

4. **Archive the completed task:**
   - **Cut** the entire task entry from `orchestration/tasks.yaml`.
   - **Paste** it into `orchestration/completed_tasks.yaml` under the `completed_tasks:` list.
   - Ensure the task is **no longer present** in `tasks.yaml`.
   - Commit both files together:
```bash
git add orchestration/tasks.yaml orchestration/completed_tasks.yaml
git commit -m "Archive P1-XXX: move completed task to completed_tasks.yaml"
git push origin main
```

> **Why:** Keeping `tasks.yaml` lean improves readability and reduces token usage when agents read the active registry. The archive preserves full history for reference.

**Final artifacts in `runs/`:**
```
orchestration/runs/
  P1-XXX_plan.md       — Planner output (Step 3)
  P1-XXX_run.md        — Worker output (Steps 4–7)
  P1-XXX_review.md   — Reviewer output (Step 8)
```

If multiple review rounds were needed:
```
  P1-XXX_run_v1.md
  P1-XXX_review_v1.md
  P1-XXX_run_v2.md
  P1-XXX_review_v2.md
```

---

## Workflow Summary Diagram

```
┌─────────────┐
│ 1. Read YAML│
└──────┬──────┘
       ▼
┌─────────────┐
│ 2. Git Branch│
└──────┬──────┘
       ▼
┌─────────────┐     ┌─────────────┐
│ 3. Planner  │────▶│ Plan .md    │
└──────┬──────┘     └─────────────┘
       ▼
┌─────────────┐     ┌─────────────┐
│ 4. Worker   │────▶│ Run .md     │
└──────┬──────┘     └─────────────┘
       ▼
┌─────────────┐
│ 5. Build    │
└──────┬──────┘
       ▼
┌─────────────┐
│ 6. Test     │
└──────┬──────┘
       ▼
┌─────────────┐
│ 7. Commit   │
└──────┬──────┘
       ▼
┌─────────────┐     ┌─────────────┐
│ 8. Reviewer │────▶│ Review .md  │
└──────┬──────┘     └─────────────┘
       │
       ├─ REQUEST_CHANGES ─▶ Go back to Step 4
       │
       └─ APPROVE ─────────▶ Step 9
                            ▼
                    ┌─────────────┐
                    │ 9. Merge +  │
                    │    Summary  │
                    └─────────────┘
```

---

## Task Status Values

| Status | Meaning |
|---|---|
| `planned` | Defined but not yet ready (dependencies pending) |
| `ready` | Dependencies met; can be picked up |
| `in_progress` | Planner or Worker is actively working on it |
| `review` | Implementation complete; waiting for review |
| `done` | Reviewer approved; merged |
| `blocked` | Blocked by an external issue or unresolved dependency |

---

## Priority Rules

1. **Never skip a dependency.** A task with unfinished `depends_on` is not ready.
2. **Prefer critical over high over medium over low.** Within a phase, follow priority order.
3. **Complete a phase before starting the next.** Exception: low-risk tasks from future phases can be picked up early if they have no dependencies on incomplete earlier work.
4. **One task at a time.** Do not run multiple tasks in parallel. Each task must be reviewed before the next starts.

---

## Adding New Tasks

To add a task:
1. Assign a unique `id` (format: `P<phase>-<number>`, e.g., `P1-007`).
2. Set `phase`, `title`, `priority`, `area`, `risk`, `depends_on`.
3. Write explicit `acceptance` criteria. Each criterion must be testable or verifiable.
4. Write `implementation_rules` referencing `AGENTS.md`.
5. List `test_commands` that must pass.
6. Set `status: planned`.

Do not create tasks larger than ~8 files or ~400 lines changed. Split large work into smaller tasks.

---

## Human-in-the-Loop

This system is designed for **human oversight**:
- Humans review Planner output before Worker starts.
- Humans can override the Reviewer if they disagree.
- Humans can reprioritize tasks by editing `tasks.yaml`.
- Humans can add emergency tasks (e.g., bug fixes) at any priority.

The orchestration system is a tool, not an autopilot.

---

## Current Execution Lane

### Sprint 1 — Reliability Core (active)
1. P1-007: Dynamic map tile loading for off-center viewport ← **Next task**

### Sprint 2 — Safety Core (planned)
1. P2-001: MAVLink command ACK handling
2. P2-002: Command confirmation workflow
3. P2-003: Failsafe alerting
4. P2-004: Tamper-evident audit log
5. P2-005: Read-only/demo/operator modes

### Sprint 3 — Real GCS Core (planned)
1. P3-001: Mission protocol download
2. P3-002: Mission protocol upload
3. P3-003: Mission clear
4. P3-004: Mission ACK handling
5. P3-005: Parameter protocol download
6. P3-006: Parameter edit/search/diff

---

## Run Report Naming Convention

```
runs/
  P1-001_plan.md    — Planner output
  P1-001_run.md     — Worker output
  P1-001_review.md  — Reviewer output
```

If a task goes through multiple Worker/Reviewer rounds:
```
  P1-001_run_v1.md
  P1-001_review_v1.md
  P1-001_run_v2.md
  P1-001_review_v2.md
```
