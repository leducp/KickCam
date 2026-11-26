# KickCam

A lean C++ library to configure and stream from USB3 Vision cameras. It reads the camera's
GenICam XML to find its features, and writes frames straight into buffers the caller owns.

Status: early development. See [docs/PLAN.md](docs/PLAN.md) for the scope and the roadmap.

## Installation

For Python:

```bash
uv pip install kickcam
```

For C++, see [docs/BUILDING.md](docs/BUILDING.md) or use the Conan recipe in
[`conan/all`](conan/all/conanfile.py).

## Building

```bash
scripts/configure.sh build --with=unit_tests   # record the option set
scripts/setup_build.sh build                   # conan install + cmake configure
cmake --build build
ctest --test-dir build --output-on-failure
```

Details, options and the Python wheel build: [docs/BUILDING.md](docs/BUILDING.md).

## License

[CeCILL-C](LICENSE)
