# CPython 3.15t engineering runtime

A normal `pip install` from this repository selects the verified release wheel
for the recorded source snapshot on Linux x86_64, CPython 3.15t and glibc 2.39+.
pip performs downloading, dependency resolution and SHA256 validation.
No consumer SDK setup or extra build flag is required for this binary path.

Version `0.11.0+stablekite.1` includes the real ALSA 1.2.14 shared library and
its original configuration tree together in `pyalsaaudio_runtime`. The SDK
locates that tree relative to its loaded library. Standard system/per-user
configuration hooks and explicit ALSA preferences remain available. Normal
operation requires no ALSA environment override.

PCM methods own native-handle leases and protect Python-visible state with
CPython object critical sections. `close()` immediately closes the object to
new calls. A native operation already in progress retains its handle until it
returns; the final call then performs the real close once. Blocking I/O and
format changes are serialized independently, with Python callbacks outside that
I/O lock. Normal close with no other call in flight remains synchronous.

The public Python API and all 1,835 SDK exports match the preceding engineering
build. The private PCM object layout changes and requires rebuilding the whole
extension. Four numeric setter annotations include the actual `__index__`
protocol supported by their native argument parser.

The original PEP 517 backend and its build requirements are preserved. Changed
native/build sources, another ABI/platform, explicit configuration settings or
compiler overrides use that original source build. The JSON manifest records
the source commit, artifact hash and the checked delivery metadata. Source
builds retain their ordinary development-library prerequisites.

Linux ARM64 CI exercises ordinary default-branch source installation against
the distribution's standard ALSA development SDK. The x86-64 binary path uses
the recorded private SDK and wheel. The runtime regression suite uses genuine
null/file PCM drivers, native threads and blocked FIFO I/O.

Physical mixer/default-card tests require an actual sound card. On a host with
no cards the complete original 11-case suite reports two passes and nine
hardware errors; these results are retained separately from the virtual PCM
gates. Explicitly preloading an older ALSA with `RTLD_GLOBAL` is a recorded
limitation of the binary path and is outside the qualified import profile.

Corresponding SDK source and LGPL license are recorded in
`pyalsaaudio_runtime/SOURCES.alsa-lib.txt` and `COPYING.alsa-lib`.

These are engineering dependency artifacts, not a StableKite release.
