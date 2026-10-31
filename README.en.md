# ClearSizer4MT5

[中文说明](README.md)

**ClearSizer v1.80** is a manual trading panel for MetaTrader 5. Place SL, Entry and TP levels on the chart, inspect the risk-based position size, then click **CONFIRM ORDER** to submit.

**This repository provides MQL5 source, without a compiled `.ex5`. MetaEditor compilation and native MT5/broker testing have not been completed.** The offline checks and their limits are listed below. Compile and validate on a demo account before use.

![ClearSizer design preview: compact panel, right-side per-leg forecasts and full-width risk/reward zones](docs/images/layout-preview.png)

*Design preview with illustrative prices and amounts. This is not a native MT5 screenshot or a trading record.*

## Features

- **NEW ORDER:** draw SL → Entry → TP1; automatically classify BUY/SELL and Limit/Stop for four ordinary pending order types.
- **QUICK:** draw SL → TP1; use the current Ask/Bid for a market entry.
- Risk sizing from balance or equity, rounded down to the broker's volume step, with volume, distance, directional exposure and margin checks.
- TP2/TP3 as separate order legs, fixed **1R break-even**, and scoped cancel/close actions.
- Right-aligned price labels with total/per-leg volumes and estimated SL/TP outcomes; risk/reward zones across the visible chart.
- A compact **300 × 354** default layout with DPI scaling, folding and complete-row pagination.

It does not generate signals or open trades from market movement alone. No DLLs, WebRequest, external libraries or additional network services are required; normal MT5 market and trading connectivity is still needed.

## Install and compile

Keep both files together:

```text
MQL5/Experts/ClearSizer/
├── ClearSizer.mq5
└── SizerTrade.mqh
```

1. Download or clone this repository.
2. In MT5, select **File → Open Data Folder**. Copy the repository's `MQL5/Experts/ClearSizer` directory into that data folder's `MQL5/Experts/` directory.
3. Press **F4** to open MetaEditor, open `ClearSizer.mq5`, and press **F7** to compile. Resolve compilation errors before attaching the EA. Include full errors and line numbers when reporting a problem.
4. Refresh **Navigator → Expert Advisors** in MT5 and attach **ClearSizer** to a chart.
5. Enable terminal **Algo Trading** and the EA's trading permission when submitting orders or using break-even.

Startup does not create a draft. Choose NEW ORDER or QUICK first. MT5 permits one EA per chart.

## Plan an order

### NEW ORDER: automatic pending classification

Click the main chart to place **red SL → blue Entry → green TP1**.

The first SL click can be on either side of the current quote, including inside the spread. Pending direction is determined by **SL relative to the planned Entry**:

| Direction | Entry relative to the current quote | Type |
| --- | --- | --- |
| SL < Entry: BUY | Entry < Ask | BUY LIMIT |
| SL < Entry: BUY | Entry > Ask | BUY STOP |
| SL > Entry: SELL | Entry > Bid | SELL LIMIT |
| SL > Entry: SELL | Entry < Bid | SELL STOP |

Entry must be sufficiently far from the applicable Ask/Bid and from SL. Pending SL/TP distances are checked relative to the planned Entry. BUY targets must be above Entry; SELL targets must be below it. Prices are aligned to the symbol's tick size.

The subtitle shows PENDING until Entry is established, then the classified order type. An invalid pending setup never falls back to a market order.

### QUICK: market entry

Place **SL → TP1**. SL below Bid selects BUY at Ask; SL above Ask selects SELL at Bid. SL inside the spread, equal to the quote, or too close is rejected. Entry follows the quote and is read-only. If the market crosses the locked SL before submission, the SL step must be repeated.

### Confirm, edit and cancel

- Once drawing is complete, inspect Lots, Risk, R:R and Margin. **One CONFIRM ORDER click normally submits the setup.**
- If the market crosses a pending Entry and changes Limit/Stop classification at confirmation time, that click only updates the type. Inspect it and explicitly confirm again. A queued double-click does not bypass this step.
- Each draft permits one submission attempt. It then shows **CHECK RESULT** and freezes draft prices. Check Trade/History and the Experts log before creating another setup after a partial or ambiguous result.
- Finished draft prices can be edited or their lines selected and dragged. Editing pending Entry/SL reclassifies the setup; a direction change clears old targets and requires TP1 to be placed again.
- With the chart focused, **ESC** cancels any unsubmitted draft, including Ready, and removes its lines, labels and zones. Risk and TP2/TP3/BE switches remain. ESC also disarms cancel/close confirmation; it does not cancel submitted orders, close positions or reset submission-result protection.
- **CANCEL SETUP** clears the draft. NEW ORDER/QUICK starts another draft. These actions do not modify actual trades. Folding cancels unfinished drawing.

Panel clicks, indicator subwindows and invalid prices do not complete a drawing step. ESC is chart-local: finish text editing and activate the relevant chart window if another control or window has keyboard focus.

## Risk, targets and account modes

`Risk budget = balance or equity × Risk %`. The adjacent −/+ changes risk by **0.25 percentage points**; EQUITY/BALANCE selects the basis. Use a decimal point and no thousands separators.

The engine uses `OrderCalcProfit` with a legal broker reference volume, then rounds down to the volume step. A result below minimum volume is rejected rather than rounded up. Risk % covers the current order group, not the total risk of all open trades.

TP2, TP3 and BE share one row. Disabled controls show their names; enabled controls turn green and show ON. Additional TP price fields appear above them. TP3 enables TP2; disabling TP2 also disables TP3. Extra targets extend by half the Entry-to-TP1 distance per level: a 2R TP1 produces default 3R/4R targets, which can be adjusted afterward.

- **HEDGING:** multiple targets use separate orders with the same SL and different TPs. Volume is divided into legal steps, with the remainder assigned to the last leg. Every leg must satisfy minimum/maximum volume; the engine rejects an invalid split instead of increasing risk or removing a target.
- **NETTING:** one target only. The symbol must have no existing position or pending order before opening a new setup. A net position containing another Magic ID/manual deal, or with unverifiable history, is excluded from local management and closing.

A rejected or partially filled leg, or a market request accepted without confirmed execution, stops later legs. Already accepted orders remain; the entire group is not retried automatically.

## Break-even and owned trades

**BE** has one fixed rule: after a **+1R** favorable move, try to move SL to the actual entry price. Here 1R is the original entry-to-SL price distance. For a BUY at 100 with initial SL 90, Bid reaching 110 makes 100 the target SL; SELL is symmetric.

BE only tightens SL, preserves TP and does not keep trailing afterward. Turning it off does not undo an SL move. This is price break-even, excluding fees, swaps and slippage; broker stop/freeze distances still apply.

BE applies to verifiable positions for the current symbol and `InpMagic`, including pending orders after they fill. Keep MT5, the EA, quotes and trading permission online. Missing original-risk information causes management to be skipped and logged.

When owned exposure exists, **CANCEL PENDING / CLOSE POSITIONS** appears on the main page. Hover for counts. Both require a second click within **8 seconds**, affect only the current symbol + `InpMagic`, and are separate actions: closing positions does not cancel pending orders.

## Forecast labels and zones

The right-side labels describe **draft scenarios, not live floating P/L**:

| Label | Forecast scope |
| --- | --- |
| Entry | Price, total volume, total estimated SL loss and combined TP-leg reward |
| SL | Price, total volume and estimated total loss |
| TP1/TP2/TP3 | Target price, that leg's volume and that leg's estimated reward |

For 0.45 lots split into three equal legs, each TP shows its own 0.15 lots. Amounts use account currency and include the configured `InpCostPerLot` estimate. Unavailable or invalid values show `--`; total TP reward remains unavailable before target drawing is complete.

Nearby labels are staggered; narrow windows use shorter text with full tooltips. After a submission attempt, forecasts remain a snapshot of that draft. Read actual fills and live P/L in MT5 Trade/History.

Red SL-to-Entry and blue Entry-to-farthest-TP zones span the visible main chart and update with chart and price changes. Before pending Entry is placed, the red zone temporarily uses the current quote as a visual reference. Draft objects are drawn behind candles; editing a draft never modifies an already submitted order.

## Display, lifecycle and cost settings

At 96 DPI and 1.0 scale, the default panel is **300 × 354**. Each extra TP row and the owned-trade action row adds 28 pixels. Folded size is 300 × 38.

- −/+ and `InpUIScale`: overall scale, 0.6–2.5. `InpDpiOverride=0` uses terminal DPI; a manual override can help with incorrect Wine/remote-desktop reports.
- `InpFont` defaults to installed Arial; `InpPanelX/Y` sets the top-left offset. Limited height uses PREV/NEXT pagination.
- The two status lines below the main buttons retain full status/detail/context in their tooltips. Native fonts, DPI and keyboard focus require terminal validation.
- **X removes the EA**, without closing positions or canceling pending orders. Broker SL/TP remains; local BE stops.
- One panel owns each terminal/account/symbol/Magic scope. Use distinct `InpMagic` values for independent panels.
- Timeframe/parameter changes, reload and restart reset the draft, scale, confirmations and BE switch; BE defaults to off. Submitted orders and broker stops remain; original-R records can be recovered.

`InpCostPerLot` is an estimated round-trip fee per lot in account currency, default 0. The engine does not automatically estimate all commissions, swaps, slippage or gap losses. Execution prices and actual costs can differ from the forecast. Broker-supported pending expiration uses GTC, otherwise DAY; symbols requiring a specified expiry time are rejected.

## Validation and tests

| Offline check | Passed scope |
| --- | --- |
| Drawing and interaction flow | 28 groups, 846 assertions |
| Trade-engine boundaries | 9 groups, 202 assertions |
| Layout geometry model | 2,160 window/DPI/scale/target/exposure combinations |

From the repository root:

```sh
python3 tests/test_trade_offline.py
python3 tests/test_drawing_offline.py
python3 tests/check_compact_layout.py
```

The first two require Python 3 and a C++17-capable `clang++`. They read actual MQL source, apply limited mechanical syntax conversions and execute C++ with stubbed broker/account/chart/trading APIs. The geometry check does not render native MT5 controls.

**These are not MetaEditor compilation or MT5/broker integration tests.** Actual execution, asynchronous broker behavior, native rendering and focus remain unverified. No compiled `.ex5`, return claim or performance guarantee is provided.

Demo acceptance should cover all four pending types and both QUICK directions; invalid prices, volume and margin; edit-driven reclassification and type-change reconfirmation; ESC during drawing/Ready and after submission; per-leg volumes/rewards; 1R BE; cancel/close scope; X and reload; right-side labels, narrow windows, two-line messages and full-width zones.

## Repository layout

```text
MQL5/Experts/ClearSizer/   # MQL5 panel and trade engine
docs/images/             # Design preview
tests/                   # Offline flow, engine and geometry checks
README.md                # Chinese guide
README.en.md             # English guide
```

## More documentation

[Full Chinese user guide](docs/USER_GUIDE.zh-CN.txt) · [Validation record](docs/VALIDATION.txt) · [Test harness notes](tests/README.md) · [Changelog](CHANGELOG.md)
