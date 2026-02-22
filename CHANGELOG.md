# Changelog

All notable changes to this project are documented in this file.

## [2.0.4] - 2026-02-22

### Changed

- Web relay defaults to `127.0.0.1` and now requires `ZCLAW_WEB_API_KEY` when binding to non-loopback hosts.
- Web relay CORS no longer uses wildcard behavior; optional exact-origin access is available via `--cors-origin` or `ZCLAW_WEB_CORS_ORIGIN`.
- Encrypted-boot startup now fails closed when encrypted NVS initialization fails, with an explicit dev-only override via `CONFIG_ZCLAW_ALLOW_UNENCRYPTED_NVS_FALLBACK`.

### Docs

- Updated relay setup docs and examples in `docs-site/getting-started.html`.
- Updated full reference README with relay CORS and encrypted-NVS startup notes in `docs-site/reference/README_COMPLETE.md`.

### Tests

- Added host tests covering origin normalization, loopback host detection, and non-loopback bind validation in `test/host/test_web_relay.py`.
