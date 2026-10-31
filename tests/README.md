# Offline checks

Run from the repository root with Python 3 and a C++17 compiler:

```sh
python3 tests/test_trade_offline.py
python3 tests/test_drawing_offline.py
python3 tests/check_compact_layout.py
```

The scripts locate the checked-in sources at
`MQL5/Experts/ClearSizer/ClearSizer.mq5` and `SizerTrade.mqh`, independently of
the shell's working directory. They use Python's standard library only.
`clang++` is the default compiler. Set `CXX` to another compiler executable
or its path; do not include flags in that environment variable.

Both C++ checks mechanically translate the actual MQL source and run it with
in-memory broker/chart stubs. Each run writes generated headers and its binary
into a fresh system temporary directory, removed on success or failure. No
generated headers or executables belong in the repository. The test-only
`mql_ulong` alias avoids collisions with platform `ulong` typedefs.

The v1.80 baseline is 202 core assertions, 846 drawing/state assertions, three
additional drawing source checks, and 2,160 layout-model combinations. Keep the
assertions in sync when the source intentionally changes.

These checks are **not** a MetaEditor/MQL5 compilation, native MT5 execution,
broker integration test, or evidence of live-trading correctness. Chart pixels
and font measurements are synthetic. Native compilation and demo-account
validation remain separate steps.

Preview rendering is not part of CI. Commit the static preview PNG instead;
these checks do not need Pillow or any preview-generation dependency.
