# Changelog

Notable changes are grouped by release. See [all releases](https://github.com/CoreTrace/coretrace-log/releases) for older versions.

## Unreleased

- Added repository contribution, conduct and security documentation.
- A child of `fork()` no longer hangs on its first line when another thread of the parent was
  deciding whether stderr takes colors, writing a line or initializing the logger at the time of
  the fork: `fork()` waits for the line and the initialization.
- A child of `fork()` logs its own pid instead of the parent's.
- An empty `NO_COLOR` no longer disables colors, as no-color.org specifies.

## v1.1.0

- Logging to sinks is serialized, including format-error fallback and sink changes.
- Recursive logging from a sink no longer deadlocks.
- Module filtering and environment defaults behave consistently before the first log.
- `enable_module()` now reports whether a module name was accepted.

[Full v1.1.0 notes](https://github.com/CoreTrace/coretrace-log/releases/tag/v1.1.0).
