#!/usr/bin/env python3
"""Compile the actual MQL engine with mechanical C++ substitutions + broker stubs.

This checks engine branches offline. It is NOT a MetaEditor/MQL5 compilation,
MT5 execution, integration test, broker simulation, or proof of live correctness.
Generated files and binaries use a fresh temporary directory, removed after each run.
"""
from pathlib import Path
import hashlib
import os
import tempfile
import re
import subprocess

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "MQL5/Experts/ClearSizer/SizerTrade.mqh"
TESTS = Path(__file__).resolve().parent
CXX = os.environ.get("CXX", "clang++")
source = SRC.read_text(encoding="utf-8")
translated = source.replace("ulong tickets[];", "std::vector<ulong> tickets;")
translated = re.sub(r"\(string\)(tickets\[i\]|ticket)", r"std::to_string(\1)", translated)
translated = re.sub(r"\bulong\b", "mql_ulong", translated)
with tempfile.TemporaryDirectory(prefix="clearsizer-core-") as build_name:
    build = Path(build_name)
    target = build / "SizerTrade.offline.hpp"
    target.write_text(translated, encoding="utf-8")
    print("Source SHA256:", hashlib.sha256(source.encode()).hexdigest(), flush=True)
    subprocess.run([CXX, "-std=c++17", "-Wall", "-Wextra", "-Wno-unused-parameter",
                    "-I", str(build), str(TESTS / "test_trade_offline.cpp"),
                    "-o", str(build / "test_trade_offline")], check=True, cwd=build)
    subprocess.run([str(build / "test_trade_offline")], check=True, cwd=build)
