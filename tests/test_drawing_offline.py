#!/usr/bin/env python3
"""Exercise mechanically extracted drawing functions from the current MQL UI.

This is a C++ branch harness with in-memory chart/broker stubs. It is not a
MetaEditor compile, MT5 event integration test, or live-trading validation.
"""
from pathlib import Path
import hashlib
import os
import tempfile
import re
import subprocess

ROOT = Path(__file__).resolve().parent.parent
TESTS = Path(__file__).resolve().parent
CXX = os.environ.get("CXX", "clang++")
ui = (ROOT / "MQL5/Experts/ClearSizer/ClearSizer.mq5").read_text(encoding="utf-8")
trade = (ROOT / "MQL5/Experts/ClearSizer/SizerTrade.mqh").read_text(encoding="utf-8")

def function(name):
    match = re.search(r"^(?:void|bool|double|int|string) " + name + r"\([^\n]*\)\s*\{", ui, re.M)
    if not match:
        raise RuntimeError("Cannot extract function " + name)
    # These functions have no brace characters inside strings or comments.
    opening = ui.index("{", match.start())
    depth = 1
    cursor = opening + 1
    while depth:
        depth += (ui[cursor] == "{") - (ui[cursor] == "}")
        cursor += 1
    return ui[match.start():cursor]

def convert(source):
    source = source.replace("ulong tickets[];", "std::vector<ulong> tickets;")
    source = re.sub(r"\(string\)(tickets\[i\]|ticket|InpMagic)", r"std::to_string(\1)", source)
    source = re.sub(r'("(?:\\.|[^"\\])*")\s*\+', r'string(\1)+', source)
    source = re.sub(r"\bulong\b", "mql_ulong", source)
    return re.sub(r"C'(\d+),(\d+),(\d+)'", lambda m: str(int(m[1]) | (int(m[2]) << 8) | (int(m[3]) << 16)), source)

with tempfile.TemporaryDirectory(prefix="clearsizer-drawing-") as build_name:
    build = Path(build_name)
    (build / "SizerTrade.offline.hpp").write_text(convert(trade), encoding="utf-8")
    broker = (TESTS / "test_trade_offline.cpp").read_text(encoding="utf-8").split('#include "SizerTrade.offline.hpp"')[0]
    (build / "DrawingBroker.offline.hpp").write_text(broker, encoding="utf-8")
    globals_ = ui[ui.index("CSizerTrade engine;"):ui.index("int Px(")]
    names = ["Px", "FontSize", "TextWidth", "N", "PriceText", "ModeText", "OrderTypeText", "LiveEntry", "Disarm", "Say", "HaveLock",
             "AcquireInstanceLock", "ReleaseInstanceLock", "RemovePanel", "ResetSessionState",
             "DeleteBody", "Line", "VisibleSpan", "Zone", "DisplayQuote", "LotsText", "SignedCash", "TagText", "TagHeight", "SyncTags", "SyncLines", "ClearDraftLines", "CancelDraft", "GenerateExtraTargets",
             "ToggleTarget", "NewPlan", "DirectionForSL", "ClassifyPending", "PendingEdited", "LockedSLValid", "RestartSL", "BeginEntryOrTP",
             "StopEdited", "DrawingPrice", "DrawingPointer", "Number", "Commit", "Action", "OnDeinit", "OnTick", "OnTimer", "OnChartEvent"]
    declarations, bodies = [], []
    for name in names:
        original = convert(function(name))
        signature, body = original.split("{", 1)
        declarations.append(signature.strip() + ";")
        # MQL resolves functions in any order. C++ needs prototypes; keep defaults there only.
        signature = re.sub(r'=\s*(?:"(?:\\.|[^"\\])*"|[^,)]+)', "", signature)
        bodies.append(signature + "{" + body)
    (build / "Drawing.offline.hpp").write_text(convert(globals_) + "\n\n" +
                                             "\n".join(declarations) + "\n\n" + "\n\n".join(bodies), encoding="utf-8")
    if not re.match(r"\s*ResetSessionState\(\);", function("OnInit").split("{", 1)[1]):
        raise RuntimeError("OnInit must call ResetSessionState as its first statement")
    manage_calls = re.findall(r"engine\.Manage\([^;]*\);", ui)
    if len(manage_calls) != 1 or not re.fullmatch(
            r"engine\.Manage\(\s*be_enabled\s*,\s*false\s*,\s*1(?:\.0)?\s*,\s*1(?:\.0)?\s*,\s*0(?:\.0)?\s*\);", manage_calls[0]):
        raise RuntimeError("UI must have one fixed 1R/zero-offset BE-only Manage call")
    if re.search(r"\b(?:manage_tab|trail_enabled|management_settings|trigger_r|trail_r|offset_points|InpTriggerR|InpTrailDistanceR|InpBEOffsetPoints)\b", ui):
        raise RuntimeError("Removed management inputs/state must not remain in UI source")
    print("UI source SHA256:", hashlib.sha256(ui.encode()).hexdigest(), flush=True)
    print("PASS OnInit starts with ResetSessionState (source assertion)", flush=True)
    print("PASS fixed BE-only Manage call / removed management inputs (source assertions)", flush=True)
    subprocess.run([CXX, "-std=c++17", "-Wall", "-Wextra", "-Wno-unused-parameter",
                    "-I", str(build), str(TESTS / "test_drawing_offline.cpp"),
                    "-o", str(build / "test_drawing_offline")], check=True, cwd=build)
    subprocess.run([str(build / "test_drawing_offline")], check=True, cwd=build)
