# eMule v0.72a

This repo is the canonical app source for the rebuilt v0.72a line.

## Refs

- `community/v0.72a`: imported community baseline
- `main`: maintained integration line
- accepted milestone refs are cut from `main` for `build`, `test`, and `bugfix`

## Working Model

- the first post-community commit is full source encoding normalization
- baseline and integration-safe changes land before the `build` milestone
- seam and testability work land before the `test` milestone
- correctness fixes land before the `bugfix` milestone

## Build And Test

Builds are driven from the canonical workspace materialized under
`EMULE_WORKSPACE_ROOT`, using the paired `eMulebb-setup` and `eMule-build`
repos rather than a machine-local cleanroom path.
