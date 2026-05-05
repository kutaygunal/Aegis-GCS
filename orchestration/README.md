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
| `prompts/planner.md` | Reusable prompt for the Planner agent. Produces implementation plans. |
| `prompts/worker.md` | Reusable prompt for the Worker agent. Implements exactly one task. |
| `prompts/reviewer.md` | Reusable prompt for the Reviewer agent. Reviews diffs against acceptance criteria. |
| `runs/` | Directory for run reports. Each task execution produces a log here. |

---

## Workflow

### Step 1: Pick a Task

Find the next `ready` task in `tasks.yaml`. A task is `ready` when:
- All `depends_on` tasks have status `done`
- Its phase is active (or you are explicitly prioritizing it)

The first task to run is:
> **P1-001: Formal connection state machine** (Sprint 1 — Reliability Core)

### Step 2: Run the Planner

Feed the Planner prompt (`prompts/planner.md`) with the task definition from `tasks.yaml`.

The Planner will:
- Identify files to touch
- Identify risks
- Define tests
- Produce a structured plan document

**Do not let the Planner modify code.**

### Step 3: Run the Worker

Feed the Worker prompt (`prompts/worker.md`) with:
- The task definition
- The Planner's plan

The Worker will:
- Implement exactly the planned changes
- Add or update tests
- Build and run tests
- Produce a Run Report

### Step 4: Run the Reviewer

Feed the Reviewer prompt (`prompts/reviewer.md`) with:
- The task definition
- The Worker's Run Report
- The full diff

The Reviewer will return:
- **APPROVE** → Mark task status as `done`, commit, push
- **REQUEST_CHANGES** → Return to Worker with blockers

### Step 5: Record the Run

Save the Planner plan, Worker Run Report, and Reviewer decision in `runs/`:

```
orchestration/runs/
  P1-001_plan.md
  P1-001_run.md
  P1-001_review.md
```

Update `tasks.yaml` to set `status: done` for the completed task.

### Step 6: Next Task

Return to Step 1 with the next ready task.

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
1. P1-001: Formal connection state machine ← **Next task**
2. P1-002: Structured rotating logs
3. P1-003: Diagnostic bundle export
4. P1-004: Config schema validation
5. P1-005: Config migration system
6. P1-006: CI hardening

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
