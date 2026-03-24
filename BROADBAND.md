# Broadband Upload Slot Control

## Goal

This branch keeps the broadband-oriented idea from `v0.60d-dev`: cap the normal
number of upload slots with `BBMaxUpClientsAllowed`, but do it in a way that
fits the `v0.72a` codebase and current broadband links.

The implementation is intentionally narrow:

- keep `BBMaxUpClientsAllowed` as the steady-state slot target
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

- hidden `BBMaxUpClientsAllowed` configuration key
- a steady-state soft cap for upload slots
- slow/stuck slot tracking on each uploading client
- replacement of bad slots instead of relying on runaway slot growth

## What This Branch Does Not Port

This branch does not carry over the broader old branch behavior:

- no full replay of the old slot-admission formulas
- no queue-score suppression changes
- no wider set of hidden broadband tuning knobs
- no UI/config panel work

That is deliberate. The goal is to isolate the broadband behavior change and keep
the patch maintainable on top of `v0.72a`.

## New Controller Design

### Hidden preference

`BBMaxUpClientsAllowed=<int>`

- stored in `preferences.ini`
- defaults to `12`
- clamped to `[MIN_UP_CLIENTS_ALLOWED, MAX_UP_CLIENTS_ALLOWED]`

This value is the normal broadband slot target. It is a soft cap, not a hard
ban on any temporary overflow.

### Effective upload budget

The new controller stops deriving slot count from the old `25 KiB/s` slot model.
Instead it computes an effective upload budget and derives per-slot targets from
that budget.

The effective budget is:

- configured upload capacity from `GetMaxGraphUploadRate(true)`
- limited by `GetMaxUpload()` when the user configured a finite upload limit
- limited further by UploadSpeedSense live allowance when USS is enabled

In other words:

`effectiveBudget = min(capacity, activeLimit, ussAllowance)`

Units follow the existing queue code and are kept in `KiB/s` until converted for
comparisons against byte-rate counters.

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

- fill freely up to `BBMaxUpClientsAllowed`
- keep `MAX_UP_CLIENTS_ALLOWED = 100` only as an absolute safety ceiling

Temporary overflow:

- allow at most `+2` extra slots
- only while the waiting queue is non-empty
- only while upload is below `95%` of the effective budget
- only after that underfill persists for at least `2 seconds`
- only if the existing throttler path still indicates demand for another slot

That gives the controller a small escape hatch for edge cases without returning
to unbounded growth.

### Slow/stuck slot reclamation

Each uploading client tracks a simple slow counter.

The counter is only meaningful while:

- the waiting queue is non-empty
- the upload list is already at or above the soft cap
- total upload is below `95%` of the effective budget

Slow threshold:

- smoothed client upload rate below `targetPerSlot / 3`

Counter behavior:

- `+1` on a slow sample
- additional `+5` when the sample is exactly `0`
- `-1` on a good sample, down to `0`
- considered slow enough for eviction at `180`

When a client gets a fresh upload slot, its slow counter is reset.

This keeps the feature intentionally simple:

- it does not punish normal short-term variance
- it removes clearly bad slots over time
- it helps maintain fill without opening many more slots

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

## Design Notes

This is a pragmatic broadband patch, not a new scheduling framework.

The implementation was kept deliberately local to the upload queue,
upload client state, the throttler slot limit hook, and hidden preferences. That
keeps the branch easier to review and easier to evolve if later testing shows
that some thresholds should move.
