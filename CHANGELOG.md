# Changelog

## [1.3.0] - 2026-11-08

### Added
- Support for decoding concatenated xz streams and xz stream padding.
- `gxrio::ostream`: `basic_ostream` now has a public constructor for writing
  uncompressed data to a streambuf, and the `ostream` convenience typedef exists.

### Changed
- `flush()` on a compressing stream now pushes the pending data through zlib or
  liblzma (`Z_SYNC_FLUSH`/`LZMA_SYNC_FLUSH`) and flushes the upstream.
- The header auto-detects `<lzma.h>` when `HAVE_LibLZMA` is not defined, so
  non-CMake users no longer silently lose xz support.
- `kDefaultBufferSize` raised from 256 to 8192.
- `basic_prefix_streambuf` only buffers the signature bytes (6) instead of a full
  256-byte buffer.
- Explicit `<istream>`, `<ostream>` and `<streambuf>` includes instead of relying on
  `<fstream>`.
- CI no longer installs the unused `libboost-dev` package.

### Fixed
- Re-initializing an output gzip/xz streambuf now finishes the old stream on
  the old upstream instead of flushing its buffered data to the new upstream.
- (De)compression error reporting no longer throws, so destroying a stream with
  `exceptions(badbit)` enabled can no longer call `std::terminate`. `close()`
  also releases both the compressor and the file before reporting a failure.
- Corrupt or truncated gzip/xz input now sets `badbit` instead of being
  reported as a clean end of stream.
- `overflow()` now reports failure after a compressor error instead of
  pretending the character was accepted.
- `deflateInit2()` no longer passes `Z_DEFLATED` where a memory level is
  expected.
- Negative `sgetn()` results are clamped to zero before being used as unsigned
  byte counts, and the signature-replay buffer is length-checked.
- Move assignment of `istream`, `ostream`, `ifstream` and `ofstream` now guards
  against self-assignment.
- CMake now always defines `HAVE_LibLZMA` (0 or 1) so the header's auto-detection
  cannot enable xz without the corresponding library being linked.
- Files without an integrity check (`xz --check=none`) decode correctly.
- Default-constructed `ifstream`/`ofstream` are no longer immediately in a
  failed state, and a failed `open()` sets only `failbit`.
- In builds without LibLZMA, `.xz` input and output are rejected with `badbit`
  instead of being silently read or written as uncompressed data. Rejection is
  sticky (`clear()` cannot expose the compressed bytes), and output files are not
  created or truncated.
- A failed compressor initialization no longer leaves `rdbuf()` pointing at a
  destroyed streambuf, and compression levels are clamped to the documented
  range 0–9.
- Moving an `ofstream` now preserves the configured compression level.
- Moving default-constructed or closed compression streambufs is safe, and
  `init(nullptr)` now fails cleanly instead of dereferencing a null upstream.
- Assigning between two active output gzip/xz streambufs no longer crashes: the
  old destination is flushed before the base move discards its upstream.
- Output write errors now set `badbit` on the owning stream, and `ofstream`
  move construction, move assignment, `open()` and `swap()` keep the compressor
  associated with the correct stream.
- Removed the file-wide `bugprone-use-after-move` suppression by making the base
  class moves explicit.
- The header includes all standard headers it uses, making it self-contained.
- The `[t_10]`, `[t_13]`, `[t_14]`, `[t_no_lzma]`, `[t_move_streambuf]`,
  `[d_3]`, `[d_5]` and `[d_error]` tests now exercise the behavior they claim to
  test, and `.xz` tests are guarded when LibLZMA is unavailable.
- Added `[d_reinit]`, `[d_error_exceptions]`, `[d_flush]`, `[t_ostream]`,
  `[d_trailing]`, `[d_empty]`, `[d_chunked]`, `[d_flush_error]`,
  `[d_live_level]`, `[d_random]` and `[d_differential]` tests, covering
  re-initialization, destructor error handling, flushing, empty and truncated
  streams, trailing garbage, short reads at signature/member boundaries,
  randomized round trips and a zlib differential check.
- Tests now use a unique per-process temporary directory, so concurrently
  running test executables no longer share or clobber files.
- README updated for the no-LibLZMA `.xz` behavior and the new error reporting.
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
