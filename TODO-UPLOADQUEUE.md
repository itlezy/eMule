# Upload Queue Closure TODO

## Summary

The upload queue is materially better on `feature/broadband-upload-limits`, but
the broader architectural closure work should not continue on this branch. This
note preserves the remaining closure plan so the work can be resumed later
without losing the reasoning.

## Remaining Work

### 1. Remove the disk-thread raw client/socket lifetime assumption

- Replace the current completion-path contract in `UploadDiskIOThread` so
  packet creation and socket-send dispatch do not depend on `session->client`
  and delayed socket destruction.
- Introduce a queue-owned handoff boundary for completed reads:
  disk thread finishes the file read and packages a queue-safe completion
  result, then the main/upload thread validates the current session and performs
  final client/socket send work.
- Make stale completions fully inert:
  no direct client mutation, no direct socket send, no dependence on deferred
  socket destruction timing.

### 2. Finish the queue-owned public state model

- Stop treating `US_*` upload state as anything more than a compatibility
  mirror.
- Replace remaining external semantic reads that ask "is active / activating /
  has slot / downloading" with one queue-owned view/query API that returns the
  phase and read-only visual/ranking state in one place.
- Convert remaining callers such as `BaseClient`, `ListenSocket`,
  `UploadClient`, `KnownFile`, and related controls.

### 3. Separate mutable queue decisions from published snapshots

- Stop using published snapshots as a control-plane source for scheduler answers
  when the queue already owns the mutable truth.
- Keep snapshots for UI and opportunistic readers only.
- Rework internal methods so mutable scheduling, admission, and slot-end logic
  read queue-owned state under `m_csQueueState`.

### 4. Centralize teardown and session-end semantics

- Collapse the remaining split between `HandleUploadSlotTeardown`,
  `RemoveActiveUploadSlot`, `RemoveWaitingClient`, `ClearClientQueueState`, and
  disconnect/ban-driven caller flows.
- Define one canonical queue end-state transition API that explicitly handles
  active teardown, waiting removal, full managed-client removal, and requeue
  after policy rotation.

### 5. Harden invariants for future maintenance

- Remove the remaining pattern where callers compose lifecycle meaning from
  multiple small wrapper predicates.
- Add asserts or seam coverage around:
  valid phase transitions, active/waiting container consistency, session
  presence only in `Activating`/`Active`/`Retiring`, and waiting membership only
  in `Waiting`.

### 6. Story closure hygiene

- Update the remaining upload-queue docs to reflect the final architecture and
  current closure state.
- Add a short developer-facing note near the queue/session boundary documenting
  what the disk thread may own, what only the queue/main thread may mutate, and
  what snapshots are allowed to represent.

## Closure Criteria

- No disk-thread code sends packets directly to client sockets.
- No correctness-critical code depends on deferred socket deletion timing.
- External callers do not infer queue meaning by combining multiple legacy
  predicates.
- Scheduler logic uses mutable queue truth, not published snapshots.
- All upload-slot teardown paths route through one canonical queue transition
  surface.
