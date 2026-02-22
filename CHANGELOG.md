# Changelog

All notable changes to this project are documented in this file.

The format is based on Keep a Changelog and this project follows Semantic Versioning.

## [Unreleased]

### Added
- docs-site `use-cases.html` chapter focused on practical and playful on-device assistant scenarios.
- docs-site `changelog.html` page for release notes on the website.
- top-level README links to the web changelog and repository changelog.

### Changed
- Agent system prompt now injects runtime device target and configured GPIO tool policy to reduce generic ESP32 pin-answer hallucinations.

## [2.4.1] - 2026-02-22

### Added
- Built-in persona tools: `set_persona`, `get_persona`, `reset_persona`, with persistent storage.
- Host tests for persona changes through LLM tool-calling.

### Changed
- Persona/tone changes now route through normal LLM tool-calling flow instead of local parser shortcuts.
- Runtime persona prompt/context sync improved after persona tool calls.
- System prompt clarified on-device execution, plain-text output requirement, and persistent persona behavior.
- README setup notes moved into a collapsible section.

## [2.4.0] - 2026-02-22

### Changed
- Cron/scheduling responsiveness tightened (10-second check interval).
- Telegram output format shifted toward plain text defaults.
- Release defaults tuned for reliability and response quality.

## [2.3.1] - 2026-02-22

### Added
- Expanded network diagnostics/telemetry for LLM and Telegram transport behavior.

### Changed
- Rate limits increased for better real-world usability.
- Boot/task stability thresholds adjusted (including stack guard and boot-loop thresholds).

## [2.3.0] - 2026-02-22

### Added
- Telegram backlog clear helper script for local/dev operations.

### Changed
- Telegram polling hardened (stale/duplicate update handling, runtime state handling, UX reliability).
