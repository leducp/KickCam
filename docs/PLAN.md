# KickCam plan

KickCam is a lean C++ library to configure and stream from USB3 Vision cameras. It reads the
camera's GenICam XML to find its features, like aravis does, but with a smaller scope, no glib,
and frames written straight into buffers the caller owns.

First user: sentry's camera HAL, to replace `AravisCamera` on marvin_f90c (Hikrobot MV-CB060 ego
camera, two MV-CB013 wrist cameras, on a Jetson).

## Scope

### In v1

- USB3 Vision devices only: discovery, control (GenCP), streaming.
- A GenApi subset: enough node types to read and write any standard feature on the cameras we
  run. An XML that uses a node type outside the subset fails to load, with the node type and name
  in the error.
- Caller-owned frame buffers, per-frame status, block id, device timestamp, host timestamp.
- Two command-line tools: `kickcam_dump` (list devices, dump features) and `kickcam_grab`
  (stream, report fps, drops and errors).
- A fake U3V device for tests, with fault injection.
- A Python wheel (nanobind), built with uv locally and cibuildwheel in CI. Its API (list devices,
  read features, grab into numpy) is scoped after phase 3.
- Linux (x86_64, aarch64) and macOS.

### Not in v1

- GigE Vision. The layering keeps room for a `gev` module later (see Architecture).
- The event channel, chunk data, GenTL producer/consumer interfaces.
- Full GenApi: caching modes, invalidators, polling, every node type.
- Windows: libusb needs the WinUSB driver bound to each camera by hand.
- Claiming GenICam compliance. The name GenICam is an EMVA trademark; the README says "reads
  GenICam XML", nothing more.

## Architecture

```mermaid
flowchart TD
    app[sentry HAL: KickCamCamera] --> nm[genapi::NodeMap]
    app --> st[u3v::Stream]
    nm -->|read/write at address| port[[Port interface]]
    port -.implemented by.-> ctl[u3v::Device: GenCP control]
    port -.implemented by.-> mock[test MockPort]
    ctl --> tr[[u3v::Transport interface]]
    st --> tr
    tr -.implemented by.-> usb[libusb transport]
    tr -.implemented by.-> fake[test FakeDevice]
```

One library, `kickcam`, in two parts:

- `kickcam::genapi` (`include/kickcam/genapi`, `src/genapi`): XML to node map over an abstract
  `Port`. No USB code. Tested with a mock port against XML files.
- `kickcam::u3v` (`include/kickcam/u3v`, `src/u3v`): discovery, GenCP control (implements
  `Port`), the XML from the device manifest, streaming. It talks to USB through a small
  `Transport` interface, so the fake device plugs in below it.

`genapi` never includes from `u3v`.

A later GigE Vision module would be a second `Port` and a second stream. `genapi` does not change.

### API sketch

```cpp
namespace kickcam::u3v
{
    std::vector<DeviceInfo> list_devices();               // vendor, model, serial, bus path

    class Device
    {
    public:
        Device(Transport& transport);
        Port& port();                                     // GenCP READMEM / WRITEMEM
        std::string xml();                                // from the manifest, unzipped
    };

    class Stream
    {
    public:
        Stream(Device& device, std::size_t payload_size);  // programs SIRM
        void start();                                      // enable SIRM, then AcquisitionStart
        void stop();
        void queue(std::span<uint8_t> buffer);             // caller-owned, stays valid until returned
        std::error_code wait(Frame& frame, nanoseconds timeout);
    };
}

namespace kickcam::genapi
{
    class NodeMap
    {
    public:
        NodeMap(std::string_view xml, Port& port);
        Node& node(std::string_view name);                // Integer, Float, Enumeration, ...
    };
}
```

`Frame` holds the buffer span, the valid size, a status (complete, incomplete, too large), the
block id and pixel format from the leader, the device timestamp in ns, and the host time when the
trailer arrived.

## Dependencies

| Dependency | Why | License |
|---|---|---|
| libusb-1.0 | USB access on Linux and macOS | LGPL-2.1, linked dynamically, in the wheels too (auditwheel and delocate bundle the shared library, which users can replace) |
| pugixml | XML parsing | MIT |
| zlib | inflate the zipped XML; a short single-entry zip reader sits on top | zlib |
| GoogleTest | unit tests | BSD-3 |
| nanobind, scikit-build-core | Python wheel | BSD-3, Apache-2.0 |

The C++ dependencies come from conan-center.

## References

- USB3 Vision standard (A3, free after registration).
- GenCP, GenApi and SFNC standards (EMVA, free download).
- aravis sources, to understand behaviour and known camera quirks. Read only: aravis is LGPL,
  KickCam is CeCILL-C, so no code is copied.

## Phases

Each phase ends with its tests green in CI.

### 0. Skeleton (done)

Build and tooling copied from kickmsg: `scripts/configure.sh` and `scripts/setup_build.sh`
(Conan + CMake), the Conan recipe in `conan/all`, the version header generated from git, the
Python wheel (scikit-build-core, nanobind, uv), CI on `master` (Linux and macOS tests, aarch64
cross-build, `conan create`, wheels, PyPI release on tag), CeCILL-C license, `AGENTS.md`,
`uncrustify.cfg`.

`kickcam` is an INTERFACE target that carries only `kickcam/version.h`. It becomes a static
library with the first sources in phase 1, which also adds the header install rule and the
recipe's `libs`.

```
KickCam/
  include/kickcam/{genapi, u3v}
  src/{genapi, u3v}
  tests/unit
  tools/{kickcam_dump, kickcam_grab}
  py_bindings/        nanobind module kickcam._native
  python/kickcam/     pure-Python package
  docs/
```

### 1. GenApi subset (1-2 weeks)

- `Port` interface: `read(address, span)`, `write(address, span)`.
- Node types:
  - Values: `Integer`, `Float`, `Boolean`, `Command`, `String`, `Enumeration` and `EnumEntry`.
  - Registers: `IntReg`, `MaskedIntReg`, `FloatReg`, `StringReg`, `StructReg` and `StructEntry`,
    with `Endianess`, `Sign`, `pAddress` and `pIndex`.
  - Computed: `Converter`, `IntConverter`, `SwissKnife`, `IntSwissKnife`.
  - Navigation: `Category`, `Port`.
- Indirections: `pValue`, `pMin`, `pMax`, `pInc`, `pIsAvailable`, `pIsImplemented`, `pIsLocked`,
  `pSelected`.
- Formula evaluator for SwissKnife and Converter: integer and float arithmetic, bitwise and logic
  operators, comparisons, the `?:` operator, the standard functions (`ABS`, `SQRT`, `TRUNC`, ...),
  `PI` and `E`.
- No cache: every read goes to the port. Feature access happens at setup, not per frame.
- Tests: formula evaluator on its own; node maps from small synthetic XMLs over a mock port,
  one per node type and indirection.

### 2. Fake device and U3V control (1 week)

- `Transport` interface: synchronous bulk out/in for control, asynchronous bulk in for the stream.
- `FakeDevice`: a register map, GenCP over the fake control endpoint, the manifest and a zipped
  XML. Fault injection: `PENDING_ACK`, wrong request id, short answers, timeouts.
- libusb transport: find U3V devices (interface class 0xEF, subclass 0x05; protocol 0 control,
  2 streaming), claim the interfaces, clear halts on open.
- GenCP: `READMEM` / `WRITEMEM` with request ids, `PENDING_ACK`, transfers split to the size
  limits read from SBRM.
- ABRM and SBRM: identity, serial, manifest table, timestamp increment.
- `kickcam_dump` works on the real cameras.

### 3. U3V streaming (1-2 weeks)

- Program SIRM from the payload size, leader and trailer sizes and the endpoint's max packet
  size: payload transfer size and count, final transfer 1 and 2.
- Keep the caller's buffers queued as libusb asynchronous transfers. Parse the leader (`U3VL`)
  and trailer (`U3VT`). Report a frame as incomplete or too large rather than dropping it
  silently.
- Recovery: resync on a leader or trailer out of order, clear halt after a stall, report an
  unplug as an error from `wait`.
- `FakeDevice` streams frames with injected faults: short transfer, missing trailer, oversized
  payload, stall.
- `kickcam_grab` streams from one or several cameras and reports fps, drops and errors.

### 4. Bench validation (1 week, overlaps phase 5)

- XML check: load the XML of every camera we have (Hikrobot MV-CB060 and MV-CB013, plus any
  other vendor at hand) and read every feature without error.
- marvin_f90c rig: ego at 3072x2048 and two wrists at 30 fps, running for 24 h, no lost frames
  under steady load.
- Unplug and replug while streaming: the stream reports the error, and reopening works.

### 5. sentry integration (1 week)

- `KickCamCamera` in sentry-core's camera HAL, replacing `AravisCamera` with the same config
  keys.
- Frames written directly into the outgoing buffers, which removes the `memcpy` in today's grab
  path.
- Once marvin_f90c runs on KickCam, delete `AravisCamera` and the aravis dependency.

Total: 4 to 6 weeks for one person.

## Risks

- **Stream sizing.** Wrong SIRM transfer sizes fail on some cameras and hosts only. Check them on
  the Jetson early, in phase 3, not at the end.
- **usbfs memory.** Linux limits the memory of in-flight USB transfers (`usbfs_memory_mb`, 16 MB
  by default). Three cameras with four 6 MB buffers each go over it. The robot image must raise
  it; `kickcam_grab` prints a clear error when the limit is hit.
- **Vendor XML licensing.** Vendor XMLs may be copyrighted, so they stay out of this public repo.
  Tests use synthetic XMLs that reproduce the patterns found in real files. The full check against
  real XMLs runs locally or from a private location.
- **GenApi long tail.** A new camera may use a node type or construct outside the subset. The
  loader fails loudly, and the fix is to add that construct with a test.

## Open questions

- C++ standard: the skeleton uses C++20 like kickmsg. Move to C++23 like sentry, which would
  allow `std::expected`?
- Error model: `std::error_code` on the frame path is settled. Should setup (open, feature
  access) throw, like KickCAT, or also return errors?
- Where the private collection of vendor XMLs lives.
