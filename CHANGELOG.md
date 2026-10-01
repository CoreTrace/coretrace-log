# Changelog

Notable changes are grouped by release. See [all releases](https://github.com/CoreTrace/coretrace-log/releases) for older versions.

## Unreleased

- Added repository contribution, conduct and security documentation.

## v1.1.0

- Logging to sinks is serialized, including format-error fallback and sink changes.
- Recursive logging from a sink no longer deadlocks.
- Module filtering and environment defaults behave consistently before the first log.
- `enable_module()` now reports whether a module name was accepted.

[Full v1.1.0 notes](https://github.com/CoreTrace/coretrace-log/releases/tag/v1.1.0).
