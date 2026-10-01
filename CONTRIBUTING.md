# Contributing to CoreTrace Logger

## Local setup

The [README](README.md#development) describes the development workflow. With CMake, Ninja and a C++20 compiler installed, build and test with:

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

Run `./scripts/format-check.sh` before opening a pull request. `./scripts/format.sh` applies the project's clang-format rules. The repository uses English Conventional Commits; `./scripts/setup-dev.sh` installs its local commit-message hook.

Explain changes to the public logging API or output format and include the test commands you ran. Report security concerns privately through [SECURITY.md](SECURITY.md).
