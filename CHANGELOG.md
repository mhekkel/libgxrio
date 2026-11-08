# Changelog

## [1.3.0] - 2026-11-08

### Fixed
- `basic_ifstream::open()` no longer leaves a stale decompressor behind when a
  non-compressed file is opened in a build without LibLZMA (the `else` branch and
  `m_upstream` assignment were inside `#if HAVE_LibLZMA`).
- Builds without LibLZMA compile again: `basic_prefix_streambuf` (which has no lzma
  dependency) is no longer wrapped in `#if HAVE_LibLZMA`, and the xz-specific test
  cases are guarded accordingly.
- The installed `gxrioConfig.cmake` only does `find_dependency(LibLZMA REQUIRED)` when
  gxrio was actually built with LibLZMA.
- `basic_ofstream::set_compression_level()` on an already open compressed file is no
  longer a no-op: the level is forwarded to the active streambuf, and the gzip
  streambuf applies it immediately via `deflateParams()`.
- Destroying a never-opened or already-closed `ifstream`/`ofstream` no longer sets
  failbit.
- Gzip format sniffing now also checks the compression method byte (`0x08`, deflate),
  reducing false positives on data that starts with `1f 8b`.
- README typos fixed; installation instructions updated; documented the C++17/zlib/
  liblzma requirements, `BUILD_EXAMPLE`, and `set_compression_level()`.
- CHANGELOG: `ENABLE_EXAMPLE` corrected to `BUILD_EXAMPLE`.

### Changed
- `kDefaultBufferSize` raised from 256 to 8192.
- `basic_prefix_streambuf` only buffers the signature bytes (6) instead of a full
  256-byte buffer.
- Explicit `<istream>`, `<ostream>` and `<streambuf>` includes instead of relying on
  `<fstream>`.
- CI no longer installs the unused `libboost-dev` package.

## [1.2.0] - 2026-08-18

### Changed
- Unit tests now use Catch2 instead of Boost.Test. The tests are split into
  `test/unit-test.cpp`, `test/unit-test-gzip.cpp` and `test/unit-test-xz.cpp`
  and share a common `test/test-main.hpp` that provides the Catch2 main and a
  `--testdir` command line option.
- The test suite no longer depends on Boost.
- Added noexcept where applicable

## [1.1.0] - 2026-08-16

### Added
- `set_compression_level()` on `basic_streambuf`, `basic_ogzip_streambuf`,
  `basic_oxz_streambuf` and `basic_ofstream`, replacing the hardcoded
  compression level.
- Regression tests for `swap()`, move construction of `istream` and the
  gzip/xz streambufs, reopening with a different extension, truncated and
  corrupt input, and compression levels.
- `BUILD_EXAMPLE` CMake option that builds the `my-zcat` example.
- Clang support for the `-Wall -Wextra` warning flags.

### Fixed
- Compile errors in `basic_ifstream::swap()`/`basic_ofstream::swap()`
  (`.` on a `unique_ptr`) and in the move constructors of the gzip and xz
  input streambufs (pointer type mismatch).
- Moving an `istream` wrapping an uncompressed streambuf no longer leaves
  it without a buffer.
- Reopening an `ifstream`/`ofstream` with a different extension no longer
  leaves a stale decompressor behind.
- `swap()` now transfers the (de)compressor together with the file buffer.
- Compression format detection no longer relies on `in_avail()`, and
  truncated input reads terminate instead of hanging.
- Errors during compression are now propagated through `close()`.
- The header is now self-contained (missing standard includes added) and
  `kDefaultBufferSize` is `constexpr`.
- The example validated its command line arguments.
- Generated `gxrioConfig.cmake` is now written to the build directory
  instead of the source tree.

## [1.0.3]
- Clean up code, remove warnings, add forge actions

## [1.0.2]
- Support for concatenated gzip files.

## [1.0.1]
- Now compiles on Windows with MSVC

## [1.0.0] - 2022-08-29

Initial release.
