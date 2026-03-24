# eMule - broadband branch

The initial purpose of this project was to provide an eMule repository that is
ready to build while keeping the amount of branch-specific logic under control.
This development branch specifically focuses on providing a build that is better
suited to address nowadays file sizes and broadband availability.

Default hard-coded parameters of eMule were better suited for
small-files/slow-connections, leading to very low per-client transfer rates by
nowadays standards. The focus here is to maximize throughput for broadband
users, to optimize seeding, while still preserving the original quality and
stability of the client as much as possible.

The main feature added by this branch is the capability of limiting the upload
slots to a certain number while ensuring to make full usage of the upload
bandwidth. This branch is intentionally narrower than the old `v0.60d-dev`
branch: it keeps the core broadband upload behavior, but drops the wider set of
queue/UI experiments and the global opcode retuning.

Please read the guide below to understand the configuration parameters and the
capabilities. For the controller internals and rationale, see
[`BROADBAND.md`](BROADBAND.md).

## Installation

### eMule

Recommended to start from a recent eMule Community build, then replace the
executable with a build produced from this branch.

Be sure to make a backup of `%LOCALAPPDATA%\eMule` first, as this is still an
experimental broadband-oriented build even if the amount of code changes is kept
deliberately low.

### Optimal Settings

Really the one recommendation is to set the values of bandwidth capacity and the
upload limit properly, plus a limit of max connections if you wish so. Other
settings, as you please.

Be fair about it, the purpose is to maximize seeding, so be generous with your
bandwidth and set it as much as possible based on your connection and I/O
capabilities.

## Upload Slots Settings

**Max upload slots** are configurable from the ini file.

Just launch the eMule exe once, close it, and then edit the ini file:

Run notepad `%LOCALAPPDATA%\eMule\config\preferences.ini`

The key to edit is the following:

`BBMaxUpClientsAllowed=12`

You can adjust this limit according to your bandwidth and I/O preferences.
Suggested ranges are 6, 8, 12, 16, 24, and so on.

This setting acts as the normal broadband slot target. The controller may
briefly exceed it by a small amount if the upload pipe is still underfilled, but
it should not keep climbing toward the legacy hard ceiling.

## Broadband Settings

This branch keeps only the hidden settings that are directly relevant to the
broadband upload strategy.

|Setting|Default|Description|
|---|---|---|
|`BBMaxUpClientsAllowed`|12|Normal target for concurrent uploads. This is the steady-state slot target, not a global hard cap.|
|`BBSessionMaxTrans`|`SESSIONMAXTRANS`|Controls how much data a client may download in a single upload session. `0` disables transfer-based rotation. Values `1..100` indicate percentage of the size of the file being uploaded. Values `>100` are interpreted as absolute bytes.|
|`BBSessionMaxTime`|`SESSIONMAXTIME`|Controls how much time in milliseconds a client may keep a single upload session. `0` disables time-based rotation. Stored as 64-bit to support very long-running sessions.|

The best take to fully understand the logic is still to review the code itself,
but the intended behavior is:

- keep a small number of productive upload slots
- derive the per-slot target from the effective upload budget
- open temporary overflow slots only when the link is still underfilled
- replace persistently weak uploaders instead of compensating by opening many
  more slots

### Notes on `BBSessionMaxTrans`

Examples:

- `BBSessionMaxTrans=0`
  Disable transfer-based rotation entirely.
- `BBSessionMaxTrans=33`
  Allow roughly one third of the current file to be uploaded in a single
  session.
- `BBSessionMaxTrans=268435456`
  Allow `256 MiB` in a single session.
- `BBSessionMaxTrans=68719476736`
  Allow `64 GiB` in a single session.

### Example Settings

#### More aggressive

```ini
BBMaxUpClientsAllowed=6
BBSessionMaxTrans=33
BBSessionMaxTime=7200000
```

#### More relaxed

```ini
BBMaxUpClientsAllowed=12
BBSessionMaxTrans=68719476736
BBSessionMaxTime=10800000
```

#### Control over Max Trans and Max Time

Bear in mind that you can adjust the max trans and the max time to decide the
best upload strategy for you, by rotating clients every x MiB, every x percent
of file, or every x seconds.

```ini
BBSessionMaxTrans=268435456
BBSessionMaxTime=10800000
```

Or, as mentioned above, you can set max trans to a percentage of the file being
uploaded, like in example a third:

```ini
BBSessionMaxTrans=33
```

## Get a High ID

As you might know, eMule servers assign you a Low or a High ID based on whether
you are able to receive inbound connections. Getting a High ID remains important
to improve your overall download/upload experience.

Ensure you have UPnP active in eMule's connection settings if your setup
supports it. Some users might be behind network infrastructure that does not
support it, so alternatives such as manual port forwarding or a VPN service
with port forwarding may be needed.

Then verify that you are actually able to receive inbound connections on the
configured ports.

## Building

This repository is the broadband feature branch. Build validation on this branch
is done through the companion build workspace, with the application repo checked
out as `eMule`.

If you are interested in performing a build, use the workspace scripts that
provide the dependency layout and the Visual Studio build environment expected by
this branch.

## Summary of Changes

### Broad philosophy

The purpose of this branch is still to seed back to the ED2K network by
uploading at sensible modern rates to a limited number of clients rather than
uploading to tens of clients at very low speeds.

### What this branch changes

- adds a hidden `BBMaxUpClientsAllowed` slot target
- bases slot behavior on upload capacity, upload limit, and UploadSpeedSense
  allowance when enabled
- reclaims slow or stuck upload slots instead of relying on runaway slot growth
- adds broadband session rotation overrides through `BBSessionMaxTrans` and
  `BBSessionMaxTime`
- supports very large transfer caps and very long session caps with 64-bit
  storage
- supports `BBSessionMaxTrans=1..100` as percentage of the file being uploaded
- adds a short cooldown so slow-evicted clients do not bounce straight back into
  upload
- scales buffering decisions from the current target-per-slot instead of fixed
  old thresholds

### What this branch does not carry over from `v0.60d-dev`

- no global retuning of `SESSIONMAXTRANS`, `SESSIONMAXTIME`,
  `MAX_UP_CLIENTS_ALLOWED`, or `UPLOAD_CLIENT_MAXDATARATE` in `Opcodes.h`
- no extra hidden broadband tuning knobs such as queue boosting/deboosting
- no queue-score suppression changes
- no auto-friend management
- no restored IP2Country feature
- no wider UI column/context-menu work from the old branch

That is deliberate. The focus here is to keep the broadband controller changes
isolated and maintainable on top of `v0.72a`.

## For Linux

For Linux and other platforms, or Windows as well, please check the friend
project https://github.com/mercu01/amule

Enjoy and contribute!
