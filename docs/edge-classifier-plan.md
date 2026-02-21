# ZeroClaw On-Device Router Plan (S3)

## Status

Draft plan for branch `on-device`.

Goal: route common tool calls locally on ESP32-S3, and fall back to cloud LLM when uncertain.

Related:
- `docs/edge-only-mode-spec.md` for the no-fallback profile variant.

## Why This Plan Is Updated

This plan is aligned to the **current zclaw codebase** (not a greenfield repo):

- Current built-in tools are defined in `main/tools.c`.
- Current dynamic custom tools are persisted via `main/user_tools.c`.
- Current request loop and metrics live in `main/agent.c`.
- Default CI enforces 888 KiB binary budget in `.github/workflows/firmware-size-guard.yml`.
- Build matrix currently covers `esp32c3`, `esp32s3`, `esp32c6` in `.github/workflows/firmware-target-matrix.yml`.

## Constraints We Must Preserve

1. Default firmware path remains stable and small.
- Keep existing default builds and behavior intact.
- Keep 888 KiB size guard for default profile.

2. Safety and policy stay in existing tool handlers.
- GPIO restrictions stay enforced by `tools_gpio_*` policy checks.
- Memory key policy (`u_` prefix and sensitive-key blocks) remains enforced.
- Cron validation remains enforced in current cron handlers.

3. Unknown/low-confidence requests must fall back to LLM.
- Never execute local actions from weak predictions.

4. This is S3-focused.
- Enable only on ESP32-S3 profile initially.
- C3/C6 stay cloud-routed as today.

## Current Tool Surface (Ground Truth)

Built-in tools today:

- `gpio_write(pin, state)`
- `gpio_read(pin)`
- `delay(milliseconds)`
- `i2c_scan(sda_pin, scl_pin, frequency_hz?)`
- `memory_set(key, value)`
- `memory_get(key)`
- `memory_list()`
- `memory_delete(key)`
- `cron_set(type, action, interval_minutes?/delay_minutes?/hour?/minute?)`
- `cron_list()`
- `cron_delete(id)`
- `get_time()`
- `set_timezone(timezone)`
- `get_timezone()`
- `get_version()`
- `get_health()`
- `create_tool(name, description, action)`
- `list_user_tools()`
- `delete_user_tool(name)`

Custom tools today:

- Up to `MAX_DYNAMIC_TOOLS = 8`
- Fields: `name`, `description`, `action`
- Runtime semantics: when invoked, agent returns `Execute this action now: <action>` back into the loop for execution by built-ins
- Current user-tool schemas are empty parameter objects
- In planned edge-only profile, `create_tool` should stay available but use a structured macro-step payload (see `docs/edge-only-mode-spec.md`)

## High-Level Architecture

Use a **hybrid local router**:

1. Stage A: deterministic parser + rules
- Fast patterns for obvious forms (`turn on pin 5`, `read pin 2`, `set timezone ...`, etc.)
- Deterministic slot extraction for strict fields (pin, delay, id, timezone, etc.)

2. Stage B: tiny INT8 classifier
- Intent classification only (or intent + minimal tags later)
- Handles fuzzy language/paraphrase where rules do not match
- Returns confidence and optional slot hints

3. Stage C: confidence gate
- If confidence >= threshold and required slots validate, execute locally.
- Else: fallback to current cloud loop unchanged.

This keeps local routing safe and practical without overfitting to a full on-device NLU stack immediately.

## Custom Tools Strategy (Important)

Custom tools are dynamic, so static intent labels are insufficient.

### Proposed custom-tool routing policy

1. Deterministic name invocation first
- If user explicitly references a custom tool name (exact/normalized match), invoke that tool.
- Example forms: `run <tool_name>`, `use <tool_name>`, `call <tool_name>`.

2. Lightweight semantic selection second
- If no explicit name, compute lexical score across user tool `name + description`.
- If top score is clearly above margin, invoke that custom tool.
- If tie/weak score, fallback to cloud.

3. Keep execution semantics unchanged
- Local path should still call existing `user_tools_find` and return action text exactly as current agent does.

4. Explicit fallback triggers
- No custom tools exist.
- Multiple close matches.
- Low confidence.
- Any parsing/validation issue.

### Why this approach

- Works with dynamic runtime-created tools.
- Avoids retraining model whenever user tools change.
- Preserves current custom-tool behavior and safety.

## Model Scope (Phase 1)

Start smaller than full dual-head BIO architecture:

- Primary: intent classifier for built-ins + `CUSTOM_TOOL_CANDIDATE` + `UNKNOWN`.
- Slot extraction: rules first (regex + constrained parsing).
- Optional phase 2: add small slot tagging head if rule coverage plateaus.

Rationale:

- Faster and safer integration with existing tool validators.
- Better behavior for dynamic custom tools.
- Less model complexity and fewer edge failures.

## Proposed Intent Set (Initial)

Use intents that map directly to current handlers:

- `INT_GPIO_WRITE`
- `INT_GPIO_READ`
- `INT_DELAY`
- `INT_I2C_SCAN`
- `INT_MEMORY_SET`
- `INT_MEMORY_GET`
- `INT_MEMORY_LIST`
- `INT_MEMORY_DELETE`
- `INT_CRON_SET`
- `INT_CRON_LIST`
- `INT_CRON_DELETE`
- `INT_GET_TIME`
- `INT_SET_TIMEZONE`
- `INT_GET_TIMEZONE`
- `INT_GET_VERSION`
- `INT_GET_HEALTH`
- `INT_CREATE_TOOL`
- `INT_LIST_USER_TOOLS`
- `INT_DELETE_USER_TOOL`
- `INT_CUSTOM_TOOL_CANDIDATE`
- `INT_UNKNOWN`

## Slot Plan (Rules-First)

Slots extracted with deterministic parser for now:

- `pin`, `state`
- `milliseconds`
- `sda_pin`, `scl_pin`, `frequency_hz`
- `key`, `value`
- `type`, `interval_minutes`, `delay_minutes`, `hour`, `minute`, `action`
- `id`
- `timezone`
- `name`, `description`, `action` (for `create_tool`)

All extracted data still passes existing tool handlers for final validation.

## Runtime Integration Plan

Add a local routing module in firmware (S3 profile):

- `main/edge_router.h`
- `main/edge_router.c`
- `main/edge_model.h`
- `main/edge_model.c`
- `main/edge_tokenizer.h`
- `main/edge_tokenizer.c`
- `main/edge_slots.h`
- `main/edge_slots.c`

Agent integration point in `process_message()` (`main/agent.c`):

1. Try local route before first LLM request.
2. If local route succeeds, send result and record local metrics.
3. If local route declines/fails, continue existing cloud flow unchanged.

## Data and Training Plan

### Dataset composition

- 5k-10k synthetic examples total.
- Strong coverage for current built-in tools.
- 500+ unknown/out-of-domain examples.
- Include typo/abbrev variants for real chat behavior.

### Tokenization

- Word-level base vocab (~2k-4k tokens).
- Add normalization and lightweight fallback buckets for OOV robustness.

### Training objective

- Intent accuracy target >= 95% on held-out synthetic set.
- Unknown detection tuned for low false-positive execution.
- Calibrated confidence threshold from validation curve.

### Export

- INT8 weights + metadata blob.
- Tokenizer map and label maps in compact binary.
- Validation parity test vs native host inference.

## Memory/Size Plan

Do not impact default 888 KiB profile.

- New Kconfig gate: `CONFIG_ZCLAW_EDGE_ROUTER` default `n`.
- Enable only in S3 edge profile config (example `sdkconfig.esp32s3.edge`).
- Keep default `sdkconfig.defaults` unchanged.

Model placement:

- Load from flash blob into PSRAM at boot when edge router enabled.
- Keep activation scratch buffers bounded and preallocated.

## CI Plan

1. Keep existing workflows as-is for default profile.
2. Add separate S3 edge workflow (non-blocking initially):
- Build `esp32s3` with edge config.
- Run host parity tests for edge inference module.
3. Add size report for edge profile separately (not tied to 888 KiB default guard).

## Metrics Plan

Extend request metrics with local-routing visibility:

- `local_attempted` (bool)
- `local_routed` (bool)
- `local_intent`
- `local_confidence`
- `local_ms`
- `fallback_reason` (low_confidence, parse_fail, validation_fail, ambiguous_custom, etc.)

This lets us quantify local hit-rate and tune threshold safely.

## Phased Delivery

### Phase 0: Scaffolding + no-op router

- Add router interfaces and metrics fields.
- Router always returns fallback.
- Verify no behavior changes.

### Phase 1: Rules-only local routing

- Implement deterministic parser for top intents.
- Add custom-tool explicit-name path.
- Ship behind S3 feature flag.

### Phase 2: Tiny classifier integration

- Add model runtime and confidence gating.
- Classifier handles fuzzy intent; rules still own slot extraction.

### Phase 3: Custom-tool semantic matcher

- Add lexical scoring over user-tool descriptions.
- Add ambiguity margin + safe fallback behavior.

### Phase 4: Optimization and quality

- Improve latency and memory.
- Tune threshold with real logs.
- Decide whether slot-tag head is worth added complexity.

## Acceptance Criteria

1. Safety
- No bypass of existing tool handler validation/policies.
- Unknown/ambiguous requests reliably fall back to cloud.

2. Correctness
- Local route executes correct tool/args on curated acceptance set.
- Custom tool routing works for explicit and clear implicit cases.

3. Performance
- Local routing median latency materially below cloud round-trip.
- No regressions in existing host tests and firmware builds.

4. Isolation
- Default profile behavior, size budget, and CI remain intact.

## Risks and Mitigations

1. Dynamic custom tools create routing ambiguity.
- Mitigation: strict confidence + margin + fallback.

2. Synthetic data mismatch vs real user language.
- Mitigation: log fallback cases and retrain offline iteratively.

3. Firmware bloat if model is bundled carelessly.
- Mitigation: feature-gated S3 edge profile and separate CI budget.

4. Over-eager local execution.
- Mitigation: conservative threshold and required-slot checks before execution.

## Immediate Next Steps (Implementation)

1. Add `edge_router` interface + no-op integration in `agent.c`.
2. Add router metrics fields to existing `METRIC request` log path.
3. Build rules-only parser for `gpio_write`, `gpio_read`, `delay`, `memory_get/set`, `cron_list/delete` first.
4. Add explicit custom-tool invocation path (`run/use/call <name>`).
5. Add host tests for local routing decisions and fallback reasons.
