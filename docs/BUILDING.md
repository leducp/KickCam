# Building

## Prerequisites

- A C++23 compiler: GCC 12+ on Linux, Apple Clang 15+ on macOS.
- CMake 3.15+ (3.30+ for the Python wheel).
- Conan 2.10+, and uv for the Python side. Install Conan with `uv tool install conan`.

## C++ build

The scripts install the dependencies and configure CMake together:

```bash
scripts/configure.sh build --with=unit_tests   # record the option set in build/.buildconfig
scripts/setup_build.sh build                   # conan install + cmake configure
cmake --build build -j
ctest --test-dir build --output-on-failure
```

`configure.sh build --show` prints the current options. `setup_build.sh` takes
`--build-type Debug` and `--target linux-aarch64` (cross-compilation, needs
`gcc-aarch64-linux-gnu` and `g++-aarch64-linux-gnu`).

After source changes, `cmake --build build` is enough. Rerun `setup_build.sh` when a
`CMakeLists.txt` or a Conan dependency changes.

### Options

| Feature (`--with=`) | CMake option | Description |
|---|---|---|
| `unit_tests` | `BUILD_UNIT_TESTS` | Build `kickcam_unit` (GoogleTest) |
| `asan` | `ENABLE_ASAN` | AddressSanitizer |
| `ubsan` | `ENABLE_UBSAN` | UndefinedBehaviorSanitizer |
| `tsan` | `ENABLE_TSAN` | ThreadSanitizer, not with `asan` or `ubsan` |

## Python wheel

From the repository root:

```bash
uv build --wheel          # wheel in dist/
uv pip install .          # or install into the active environment
```

CI builds the released wheels with cibuildwheel (manylinux_2_28 x86_64 and aarch64, macOS
universal2), using uv as the build frontend. A tag `vX.Y.Z` on `master` publishes them to PyPI.

## Conan package

```bash
conan create conan/all --version=0.0.0 --build=missing -s compiler.cppstd=23
```
