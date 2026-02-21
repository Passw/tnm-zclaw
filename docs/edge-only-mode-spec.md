# ZeroClaw Edge-Only Mode Spec (No LLM Fallback)

## Purpose

Define a separate firmware mode for ESP32-S3 that performs local routing/execution only, with **no cloud LLM calls and no fallback path**.

This mode is opt-in and intentionally separate from default zclaw behavior.

## Product Positioning

`edge-only` is a different operating mode, not just a threshold tweak:

- Default mode: cloud LLM orchestration (current behavior)
- Edge-only mode: deterministic + tiny on-device router, local tool execution only

Users should explicitly choose this mode during install/flash.

## Hardware Scope

Initial target: **ESP32-S3 only** (e.g., XIAO ESP32-S3 Sense).

- `esp32c3` and `esp32c6` remain on current cloud-first firmware path.
- Edge-only is a distinct S3 profile artifact.

## Non-Goals (v1)

- No dynamic cloud fallback when intent is uncertain.
- No freeform natural-language custom actions in edge-only profile.
- No voice/ASR in this phase.

## Mode Contract

In edge-only mode:

1. Incoming user input is processed by local router.
2. Router either:
- maps to a supported tool call and executes, or
- returns deterministic unsupported/ambiguous response.
3. System never calls provider APIs.
4. Provisioning does not require LLM backend/model/API key.

## Build/Profile Design

## New profile name

- `edge-only-s3`

## New Kconfig flags (proposed)

- `CONFIG_ZCLAW_EDGE_ROUTER` (bool, default `n`)
- `CONFIG_ZCLAW_EDGE_ONLY` (bool, default `n`)
- `CONFIG_ZCLAW_EDGE_ONLY_DISABLE_LLM` (bool, default `y` when edge-only)
- `CONFIG_ZCLAW_CREATE_TOOL_MODE_LEGACY` (bool)
- `CONFIG_ZCLAW_CREATE_TOOL_MODE_MACRO` (bool)

Behavioral rule:

- `CONFIG_ZCLAW_EDGE_ONLY=y` implies `CONFIG_ZCLAW_EDGE_ROUTER=y` and compile-time exclusion of outbound LLM request path.
- `CONFIG_ZCLAW_EDGE_ONLY=y` implies `CONFIG_ZCLAW_CREATE_TOOL_MODE_MACRO=y`.
- Default/cloud builds keep `CONFIG_ZCLAW_CREATE_TOOL_MODE_LEGACY=y`.

## Build artifacts

- Keep default artifact naming unchanged for existing builds.
- Add separate output artifact for edge-only profile (e.g., `zclaw-edge-only-s3.bin`).

## Runtime Behavior Changes

## Agent loop

Current `agent.c` flow does LLM request/response tool loop.

Edge-only flow should:

1. Attempt local routing first and only.
2. If local route succeeds, execute tool and return result.
3. If local route fails, return deterministic message:
- `"Unsupported in edge-only mode. Try a supported command or reflash default mode."`

No `llm_request()` call path in edge-only.

## Startup/provisioning dependencies

Edge-only should still support:

- WiFi connection (optional, for relay/Telegram if user wants remote interface)
- cron/NTP/timezone behavior
- Telegram path if configured

But should not require:

- `llm_backend`
- `llm_model`
- `llm_api_key`

## Tool Support Matrix (v1)

Support a deliberate subset first, then expand.

### Supported in edge-only v1

- `gpio_write`
- `gpio_read`
- `delay`
- `i2c_scan`
- `memory_set`
- `memory_get`
- `memory_list`
- `memory_delete`
- `cron_set`
- `cron_list`
- `cron_delete`
- `get_time`
- `set_timezone`
- `get_timezone`
- `get_version`
- `get_health`
- `create_tool` (edge-only schema variant; see below)
- `list_user_tools`
- `delete_user_tool`

## Custom Tools Redesign for Edge-Only

Current custom tool format:

- `name`, `description`, `action` (natural language string)

This legacy payload is incompatible with no-LLM mode.

## `create_tool` is build-conditional (same tool name, different payload)

Keep `create_tool` present in all builds, but gate semantics by profile:

- Default/cloud builds: current legacy payload (`name`, `description`, `action`)
- Edge-only builds: structured payload (`name`, `description`, `steps`)

This preserves tool identity for users while keeping edge-only deterministic.

## Proposed edge-only `create_tool` payload: macro steps

Add structured macro representation for edge-only:

```c
typedef struct {
    char name[TOOL_NAME_MAX_LEN];
    char description[TOOL_DESC_MAX_LEN];
    uint8_t step_count;
    macro_step_t steps[MAX_MACRO_STEPS];
} macro_tool_t;

typedef struct {
    char tool_name[24];         // must be built-in supported tool
    char args_json[160];        // compact validated JSON args
} macro_step_t;
```

External tool names remain unchanged:

- `create_tool(...)`
- `list_user_tools()`
- `delete_user_tool(name)`

Internally, edge-only persists/executes structured macro steps.

Execution model:

- Router maps request to a macro tool by explicit name or high-confidence match.
- Runtime executes steps sequentially via existing `tools_execute()`.
- Any step failure aborts macro and returns error.

### Backward compatibility

In edge-only mode:

- If legacy payload (`action` string only) is sent, return:
- `"create_tool in edge-only mode requires structured steps; freeform action is unsupported."`

## Safety and Validation

All local routes must reuse existing handler validation:

- GPIO allowlist/range checks in `tools_gpio.c`
- Memory key policy checks in `tools_memory.c`
- Cron/timezone validators in `tools_cron.c`

Router should not bypass these checks by writing directly to device state.

## Router Confidence Policy

Even without cloud fallback, router must be conservative:

1. If confidence below threshold -> unsupported response.
2. If required slots missing -> unsupported response.
3. If multiple custom macro matches are close -> ambiguous response.

Never guess-execute on weak confidence.

## UX and Scripts

## install.sh changes (proposed)

Add profile argument:

- `--profile default|edge-only-s3`

Edge-only profile behavior:

- sets target `esp32s3`
- builds with edge-only sdkconfig/profile
- flashes edge-only artifact
- runs provisioning in edge-only mode

## provision.sh changes (proposed)

Add:

- `--mode default|edge-only`

Edge-only provisioning prompts:

- WiFi SSID/pass
- Telegram token/chat id (optional)
- timezone (optional)

Skip prompts and NVS writes for:

- backend/model/api-key

## Docs messaging

Clearly state:

- Edge-only mode has no AI fallback.
- Command set is finite.
- Ambiguous requests will be rejected, not inferred.

## CI and Release

## CI workflows

Keep current workflows unchanged for default path:

- Host tests
- Firmware target matrix
- 888 KiB size guard

Add new workflows for edge-only profile:

1. `firmware-edge-only-s3.yml`
- Build `edge-only-s3` artifact
- Run size report with separate threshold

2. `host-edge-router-tests.yml`
- Run parser/router unit tests
- Run macro-tool execution tests

## Size policy

Do not apply default 888 KiB guard to edge-only artifact.

Define separate threshold for edge-only profile once measured (e.g. `<= 1.2 MiB` initial cap, tune down later).

## Telemetry / Metrics

Extend metrics with edge-only outcomes:

- `edge_mode=1`
- `local_routed=1|0`
- `route_intent=<id|name>`
- `route_confidence=<float>`
- `route_ms=<u32>`
- `reject_reason=low_confidence|missing_slot|ambiguous|unsupported_intent`

These metrics replace cloud fallback analytics in this mode.

## Test Plan

## Host tests

- Deterministic parser unit tests by intent
- Confidence gate tests
- Unsupported/ambiguous response tests
- Macro creation/validation tests
- Macro execution step/failure tests

## Device tests (S3)

- End-to-end latency for routed commands
- Memory usage under repeated commands
- Behavior with empty/malformed input
- Behavior with legacy `create_tool` call

## Migration Plan

### Phase A

- Introduce profile + no-op edge router (reject all)
- Ensure no-LLM path is compile-clean

### Phase B

- Implement rules-only routing for top intents
- Add deterministic unsupported responses

### Phase C

- Add tiny classifier for fuzzy intent mapping
- Add macro tool subsystem and routing

### Phase D

- Optimize latency/size
- tighten size threshold

## Open Decisions

1. Should Telegram remain enabled in edge-only mode by default, or opt-in only?
2. Should `run_macro_tool(name)` exist as a first-class tool, or rely on NL router invocation only?
3. Should edge-only provisioning include a mandatory capability banner summary after flash?

## Summary

Edge-only mode is technically sound if treated as a separate S3 profile with strict behavior boundaries.

The critical design decision is custom tools:

- legacy freeform `create_tool` depends on LLM orchestration
- edge-only requires structured macro tools
- `create_tool` stays available in both builds, but payload/semantics are build-specific

With that change, zero-fallback local mode becomes coherent, testable, and safe.
