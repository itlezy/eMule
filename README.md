# eMule v0.72a

This repo is the canonical app source for the rebuilt v0.72a line.

## Refs

- `community/v0.72a`: imported community baseline
- `bb/v0.72a/main`: maintained integration line
- accepted milestone refs are cut from `bb/v0.72a/main` for `build`, `test`, and `bugfix`

## Working Model

- the first post-community commit is full source encoding normalization
- baseline and integration-safe changes land before the `build` milestone
- seam and testability work land before the `test` milestone
- correctness fixes land before the `bugfix` milestone

## Build And Test

Builds are driven from the local release workspace under:

`C:\prj\p2p\eMule\workspaces\v0.72a`
