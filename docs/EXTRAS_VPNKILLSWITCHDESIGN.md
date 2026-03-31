# External VPN Kill Switch Design

**Status:** Design / Planning
**Scope:** External helper tool, not an in-process eMule feature

---

## 1. Overview

Instead of adding a bind-failure kill switch directly inside eMule, the preferred direction is a **separate local watchdog tool** that monitors VPN connectivity and **shuts down running P2P applications** if the VPN is no longer available.

The goal is to keep the safety policy **outside** the eMule process, so it can protect:

- `eMule.exe`
- future sidecars or helper tools
- other configured P2P applications

This design intentionally avoids coupling VPN safety policy to eMule startup, bind resolution, or shutdown logic.

---

## 2. Core Behavior

When the configured VPN is healthy, the helper does nothing.

When the configured VPN is no longer healthy, the helper must:

1. detect the failure quickly
2. identify configured P2P processes which are currently running
3. request a graceful shutdown first
4. escalate to forced termination if a target process does not exit in time
5. write a local audit log explaining what was detected and what action was taken

The helper should be **fail-closed** from a policy perspective:

- if the VPN state cannot be confirmed, treat that as unsafe
- if a protected process keeps running after graceful shutdown, terminate it

---

## 3. Detection Model

The watchdog should not rely on a single weak signal like “adapter exists”.

Preferred VPN health checks:

- configured VPN interface is present
- configured VPN interface is operational and has the expected IPv4 address family state
- the configured VPN interface still owns the expected local address, when one is pinned
- optional route check confirms the default or selected egress route still points through the VPN

Recommended implementation shape:

- monitor by interface identifier or friendly name, not just current IP text
- optionally pin an expected local VPN IP as a stricter second check
- poll at a low interval such as `1s` to `3s`

This is acceptable for a separate helper because the polling cost is low and the logic stays isolated from eMule.

---

## 4. Protected Process Handling

The tool should support a configured list of protected processes, with `eMule.exe` as the first-class default target.

Recommended handling sequence:

1. discover matching running processes
2. attempt graceful close
3. wait a short bounded timeout
4. force-kill remaining targets

Recommended defaults:

- graceful close first
- forced kill after timeout
- configurable process allowlist, for example:
  - `emule.exe`
  - additional P2P clients if the operator wants broader protection

The helper should not attempt app-specific in-process cleanup. It should act like an external safety controller.

---

## 5. Suggested Tool Shape

A standalone helper is preferred over an eMule-integrated implementation.

Acceptable forms:

- PowerShell watchdog script for fast local iteration
- Python helper for easier packaging and richer state handling
- small native Windows console tool if a permanent low-friction deployment is desired later

Recommended first implementation:

- PowerShell or Python prototype
- config file holding:
  - VPN interface identifier/name
  - optional expected VPN IPv4
  - poll interval
  - protected process names
  - graceful-exit timeout
  - log file path

---

## 6. Logging and Evidence

The helper should keep its own log separate from eMule logs.

Minimum logged events:

- watchdog startup
- configured VPN target
- current VPN health transitions
- detected protected processes
- graceful-close attempt
- forced termination
- watchdog shutdown

This is important because VPN failures are operational incidents, not just application events.

---

## 7. Out of Scope

This design does **not** add:

- an in-app eMule bind kill switch
- eMule UI controls for VPN enforcement
- route or firewall manipulation
- automatic VPN reconnection logic

Those can be considered later, but they are separate from this external watchdog design.

---

## 8. Decision

The current direction is:

- keep eMule focused on binding and diagnostics
- implement VPN safety as a separate external tool
- treat the external watchdog as the authoritative kill-switch layer for P2P applications
