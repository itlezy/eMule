# FEAT_001 — Broadband Upload Slot Control [DONE]

## Table of Contents

- [Goal](#goal)
- [Problem in Stock v0.72a](#problem-in-stock-v072a)
- [Why the Legacy Logic No Longer Works Well](#why-the-legacy-logic-no-longer-works-well)
- [What Was Useful in v0.60d-dev](#what-was-useful-in-v060d-dev)
- [What This Branch Keeps](#what-this-branch-keeps)
- [What This Branch Does Not Port](#what-this-branch-does-not-port)
- [New Controller Design](#new-controller-design)
- [Expected Outcome](#expected-outcome)
- [Files Touched](#files-touched-by-the-implementation)
- [Design Notes](#design-notes)

## Goal

This branch keeps the broadband-oriented idea from `v0.60d-dev`: cap the normal
number of upload slots with `BBMaxUpClientsAllowed`, but do it in a way that
fits the `v0.72a` codebase and current broadband links.

The implementation is intentionally narrow:

- keep `BBMaxUpClientsAllowed` as the steady-state slot target
- override the legacy session rotation defaults with `BBSessionMaxTrans` and `BBSessionMaxTime`
- base slot decisions on the actual upload budget
- allow only small temporary overflow when the upload pipe is still not full
- reclaim obviously weak upload slots instead of compensating by opening many more

It does not attempt to replay the whole historic broadband branch.

## Problem in Stock `v0.72a`

The stock `v0.72a` upload controller grows the number of upload slots almost
linearly with available upload speed.

The key legacy assumption is that each upload slot should be driven toward a
small target rate, effectively capped by `UPLOAD_CLIENT_MAXDATARATE = 25 KiB/s`.
That assumption made sense on older uplinks where many low-rate slots were
needed to keep the pipe busy. It does not fit a modern broadband uplink.

### Worked example: `50 Mbit/s`

`50 Mbit/s` is about `6100 KiB/s`.

With the legacy logic:

- target per slot is capped around `25 KiB/s`
- desired slot count becomes roughly `6100 / 25 = 244`
- the queue keeps accepting more clients until it reaches the absolute safety cap
- on `v0.72a`, that means it trends toward `MAX_UP_CLIENTS_ALLOWED = 100`

This is why a modern broadband link can keep growing slot count even when a much
lower number of well-performing slots would already fill the connection.

## Why the Legacy Logic No Longer Works Well

There are three structural issues with the stock logic on high-bandwidth links.

### 1. Slot count scales almost directly with bandwidth

The legacy formulas in `UploadQueue.cpp` and `UploadBandwidthThrottler.cpp`
derive slot count from total upload speed divided by a low per-client target.
As the available upload budget rises, slot count rises with it.

### 2. The per-slot target is tuned for old links

The old controller effectively assumes that a healthy upload slot should be in
the low tens of KiB/s. On a `50 Mbit/s` uplink, twelve slots would each need to
carry roughly `500 KiB/s`, which the stock logic treats as far too high. The
controller responds by creating more slots, not by keeping a smaller set of
stronger ones.

### 3. Weak slots are not retired as a first-class control signal

If one or more uploading clients are weak, stuck, or effectively idle, the stock
response is mostly to keep pressure on opening more slots. That fills the pipe by
accumulation rather than by replacing bad slots with better ones.

## What Was Useful in `v0.60d-dev`

The useful idea in `v0.60d-dev` was not the full historic implementation. The
useful idea was:

- keep a configurable broadband slot target
- do not let slot count grow without bound
- detect persistently weak uploaders
- recycle weak slots when there are better waiting clients

That behavior maps well to the goal on `v0.72a`.

## What This Branch Keeps

This branch keeps the following parts of the old broadband approach:

- hidden `BBMaxUpClientsAllowed` configuration key as the steady-state slot target
- hidden `BBBoostLowRatioFiles`, `BBBoostLowRatioFilesBy`, and `BBDeboostLowIDs` queue-score controls for strict seeders
- hidden `BBSessionMaxTrans` and `BBSessionMaxTime` overrides for broadband session rotation
- restored `IP2Country` backend support for client country lookups
- `All-Time Ratio` / `Session Ratio` columns in shared, upload, and queue lists
- `Cooldown` column in upload and queue lists
- a steady-state soft cap for upload slots
- slow/stuck slot tracking on each uploading client
- replacement of bad slots instead of relying on runaway slot growth
- low-ratio preference when ordering the shared-file list published to servers,
  while still respecting upload priority first

## What This Branch Does Not Port

This branch does not carry over the broader old branch behavior:

- no full replay of the old slot-admission formulas
- no wider set of hidden broadband tuning knobs beyond the slot target, score bias, and session rotation overrides
- no separate broadband preferences page; the current branch exposes the kept
  broadband controls on `Preferences > Tweaks > Broadband`

That is deliberate. The goal is to isolate the broadband behavior change and keep
the patch maintainable on top of `v0.72a`.

## New Controller Design

### Broadband preferences

`BBMaxUpClientsAllowed=<int>`

- stored in `preferences.ini`
- defaults to `12`
- clamped to `[MIN_UP_CLIENTS_ALLOWED, MAX_UP_CLIENTS_ALLOWED]`

This value is the normal broadband slot target. It is a soft cap, not a hard
ban on any temporary overflow.

`BBSessionMaxTrans=<uint64>`

- stored in `preferences.ini`
- defaults to `SESSIONMAXTRANS`
- overrides the stock one-chunk session cap on this branch
- semantics:
  - `0` disables transfer-based session rotation
  - `1..100` means percent of the currently uploaded file size
  - `>100` means an absolute byte limit

`BBSessionMaxTime=<uint64>`

- stored in `preferences.ini`
- defaults to `SESSIONMAXTIME`
- overrides the stock one-hour session cap on this branch
- `0` disables time-based session rotation

`BBBoostLowRatioFiles=<float>`

- stored in `preferences.ini`
- defaults to `0`
- `0` disables the low-ratio bias
- when enabled, files whose all-time uploaded-bytes-to-file-size ratio is below
  the configured threshold get a queue-score bonus

`BBBoostLowRatioFilesBy=<float>`

- stored in `preferences.ini`
- defaults to `0`
- additive score bonus applied when the low-ratio threshold matches

`BBDeboostLowIDs=<int>`

- stored in `preferences.ini`
- defaults to `0`
- `0` and `1` disable the behavior
- values above `1` divide the queue score of actual LowID clients by the
  configured value

These settings are also exposed in `Preferences > Tweaks > Broadband` with a
friendlier editor:

- `Max Upload Clients`
- `Session Transfer Limit`
  - `Disabled`
  - `Percent of file size`
  - `Absolute limit (MiB)`
- `Enable session time limit`
  - `Minutes`
- `Enable low-ratio file boost`
  - `Ratio threshold`
  - `Score bonus`
- `Deboost LowID clients`
  - `Divisor`

### Effective upload budget

The new controller stops deriving slot count from the old `25 KiB/s` slot model.
Instead it computes an effective upload budget and derives per-slot targets from
that budget.

The effective budget is:

- configured upload capacity from `GetMaxGraphUploadRate(true)`
- limited by `GetMaxUpload()` when the user configured a finite upload limit

In other words:

`effectiveBudget = min(capacity, activeLimit)`

Units follow the existing queue code and are kept in `KiB/s` until converted for
comparisons against byte-rate counters.

Broadband-specific overflow and slow-slot logic only activate when this budget is
based on a real configured capacity. If the code only has the old
`UNLIMITED -> 16 KiB/s` fallback, it falls back to the legacy per-slot target
logic instead of pretending that `16 KiB/s` is a real line budget.

### Target per slot

The per-slot target now becomes:

`targetPerSlot = effectiveBudget / BBMaxUpClientsAllowed`

with a floor of `3 KiB/s`.

The existing minimum-admission threshold is kept at `75%` of the target value.

On a `50 Mbit/s` uplink with `BBMaxUpClientsAllowed=12`:

- effective budget is about `6100 KiB/s`
- target per slot is about `508 KiB/s`
- the controller can keep a small number of strong slots instead of chasing
  dozens of weak ones

### Slot admission

Normal behavior:

- fill freely only while the upload budget is still underfilled
- stop opening slots once the current upload is already satisfying the effective budget, even below `BBMaxUpClientsAllowed`
- keep `MAX_UP_CLIENTS_ALLOWED = 100` only as an absolute safety ceiling

Temporary overflow:

- allow a derived overflow allowance of roughly `ceil(softMaxSlots / 6)`
- only while the waiting queue is non-empty
- only while upload is underfilled by at least the larger of:
  - half of one target slot, or
  - `5%` of the effective budget
- only after that underfill persists for at least `2 seconds`
- only if the throttler explicitly reported that it still wanted another productive slot

That gives the controller a small escape hatch for edge cases without returning
to unbounded growth.

### Slow/stuck slot reclamation

Each uploading client tracks time spent in a bad state instead of a raw sample
counter.

Slow-slot tracking is only meaningful while:

- the waiting queue is non-empty
- the upload list is already at or above the soft cap
- total upload is underfilled according to the derived headroom rule above
- a real effective budget exists

Slow threshold:

- smoothed client upload rate below `targetPerSlot / 3`

Time windows:

- evict after `15 seconds` spent below the slow-rate threshold
- evict after `3 seconds` spent at exactly `0` upload rate
- good samples reduce accumulated slow time
- neutral periods freeze the timers instead of silently forgiving the client

When a client gets a fresh upload slot, its slow timers are reset.

When a client is evicted for being slow or stuck, it is still requeued
immediately for protocol compatibility, but its queue score is held at zero for
one slow-eviction window. That short cooldown prevents the same weak uploader
from bouncing straight back into the next slot.

### UI readouts

The branch surfaces the seeding policy with a small, explicit UI replay:

- `All-Time Ratio` = all-time transferred bytes for the file divided by file size
- `Session Ratio` = current-session transferred bytes for the file divided by file size
- `Cooldown` = remaining slow-upload suppression time after a weak slot was evicted

Those columns are shown in:

- shared files: `All-Time Ratio`, `Session Ratio`
- upload list: `All-Time Ratio`, `Session Ratio`, `Cooldown`
- queue list: `All-Time Ratio`, `Session Ratio`, `Cooldown`

### Queue scoring for strict seeders

The slot controller decides how many productive upload slots exist. Queue score
still decides who gets them.

This branch keeps that policy explicit instead of burying it in slot math:

- `BBBoostLowRatioFiles` and `BBBoostLowRatioFilesBy` let the queue favor files
  that have historically seen fewer uploaded copies
- the ratio metric is simple and local to the current file:
  `allTimeTransferred / fileSize`
- `BBDeboostLowIDs` optionally penalizes actual LowID clients with a score
  divisor

This is intentionally harsh. The goal of this branch is not neutral fairness; it
is to let a strict seeder spend limited upload bandwidth on the files and peers
the operator considers most useful.

This keeps the feature intentionally simple:

- it does not punish normal short-term variance
- it removes clearly bad slots over time
- it helps maintain fill without opening many more slots

### Session rotation

The branch now replaces the stock `SESSIONMAXTRANS` and `SESSIONMAXTIME`
rotation checks with the hidden broadband overrides:

- `BBSessionMaxTrans` replaces the stock one-chunk transfer cap
- `BBSessionMaxTime` replaces the stock one-hour time cap
- both are stored as `uint64`

This keeps healthy upload sessions bounded without relying on the old score-based
rotation logic, which caused extra churn on broadband-oriented low-slot setups.

### Buffering

The branch also derives buffering behavior from the current per-slot target:

- large socket send buffers are enabled once a slot reaches roughly half of the
  current target-per-slot rate
- upload disk prefetch grows from `1` to `3` to `5` blocks based on the current
  slot rate relative to that same target

That keeps queueing and I/O policy aligned with broadband slot sizes instead of
using the old fixed `75 KiB/s` and `100 KiB/s` thresholds.

## Expected Outcome

With `BBMaxUpClientsAllowed=12` on a `50 Mbit/s` link, the expected steady state
is roughly:

- normal operation around `12` slots
- brief expansion to `13..14` only when the link remains underfilled
- no long-term drift toward `100`
- better fill retention by replacing weak uploaders instead of stacking more slots

## Files Touched by the Implementation

- `srchybrid/Preferences.h`
- `srchybrid/Preferences.cpp`
- `srchybrid/UpdownClient.h`
- `srchybrid/BaseClient.cpp`
- `srchybrid/UploadClient.cpp`
- `srchybrid/UploadQueue.h`
- `srchybrid/UploadQueue.cpp`
- `srchybrid/UploadBandwidthThrottler.cpp`
- `srchybrid/UploadBandwidthThrottler.h`
- `srchybrid/UploadDiskIOThread.cpp`

## Design Notes

This is a pragmatic broadband patch, not a new scheduling framework.

The implementation was kept deliberately local to the upload queue,
upload client state, the throttler slot limit hook, and hidden preferences. That
keeps the branch easier to review and easier to evolve if later testing shows
that some thresholds should move.
