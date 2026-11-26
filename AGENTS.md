# Agent guide -- KickCam

Project-wide directives for any LLM working in this repository. Read this before proposing
changes. These override generic defaults. KickCam is a lean C++ library to configure and stream
from GenICam cameras (USB3 Vision first, GigE Vision next), reading the camera's GenICam XML.
Scope and roadmap: `docs/PLAN.md`.

## 1. Build

Use the wrapper scripts, not `cmake` or `conan` directly (see `docs/BUILDING.md`):

```bash
scripts/configure.sh build --with=unit_tests
scripts/setup_build.sh build
cmake --build build -j && ctest --test-dir build --output-on-failure
```

## 2. Keep It Simple, Stupid (KISS)

Pick the simplest design that solves the problem. Avoid helpers called from exactly one place
(inline them), mirrors of state another component owns (query the authority), dead defensive
checks on values a constructor already guarantees, and speculative abstractions with no concrete
consumer. Three similar lines beat a premature abstraction.

Stay inside the scope of `docs/PLAN.md`. A GenICam construct outside the supported subset fails
loudly at load time; support for it is added with a test, never guessed.

## 3. Correct, clear, then fast

The frame path (transfer completion, leader/trailer parsing, buffer return) is hot: no heap
allocation, no avoidable copy, no syscall beyond the USB transfers. Setup paths (discovery,
XML loading, feature access) favour clarity.

## 4. References

- USB3 Vision, GenCP, GenApi and SFNC standards are the source of truth.
- aravis may be read to understand camera behaviour. It is LGPL and KickCam is CeCILL-C: never
  copy its code.
- Vendor XML files are never committed. Tests use synthetic XMLs.

## 5. Coding style

`uncrustify.cfg` enforces the mechanical parts. Match the surrounding file.

### 5.1 Formatting

- **Allman braces**, 4-space indent, no tabs. Case labels indented inside `switch`.
- **East const**: `T const&`, `T const*`.
- **Pointer alignment left**: `T* p`.
- **Members**: `snake_case`, trailing underscore for non-public data members. Public fields: no
  underscore.
- **Types**: `PascalCase`.
- **Namespaces**: `kickcam`, `kickcam::genapi`, `kickcam::u3v`; contents indented, no closing
  `// namespace` comment.
- **Constructor initializers**: one per line, `,` leading the next line.

### 5.2 Language rules

- **Header guards, never `#pragma once`.** Format `KICKCAM_<PATH>_H` (e.g.
  `kickcam/u3v/Stream.h` -> `KICKCAM_U3V_STREAM_H`).
- **No ternary operator** in code you add or modify. Rewrite as `if`/`else`, early return, or an
  `if`-assigned variable.
- **Prefer `not` / `and` / `or`** over `!` / `&&` / `||`.
- **`override` mandatory** on overriding virtuals.
- **`inline` only with a real rationale** (`constexpr`, templates) -- bodies go in a `.cc`.
- **No `explicit` by reflex** -- only with a concrete reason.

### 5.3 Comments

Default to writing nothing. Only add a comment when a hidden constraint or invariant is
maintained, when the code works around a specific camera or platform quirk (name the device), or
when a reader would otherwise be surprised. Never restate what the code does. Never reference
the current PR or issue.

### 5.4 Other defaults

- **ASCII only** in source and docs.
- **No emojis** in code, comments, commit messages or docs.
- **Tests**: GoogleTest + gmock in `tests/unit/`, one `<name>-t.cc` per source file.
