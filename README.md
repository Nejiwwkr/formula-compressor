# Formula Compressor

[![Build](https://github.com/Nejiwwkr/formula-compressor/actions/workflows/build.yml/badge.svg)](https://github.com/Nejiwwkr/formula-compressor/actions/workflows/build.yml)
[![Release](https://img.shields.io/github/v/release/Nejiwwkr/formula-compressor)](https://github.com/Nejiwwkr/formula-compressor/releases/latest)

**Write the compression curve.**

A normal compressor is a fixed transfer curve, usually `f(x) = min(x, ax + b)`.
Formula Compressor exposes the curve: type an expression for `f(x)`, or use both
the main input `x` and an external sidechain `s` in `f(x, s)`. The result is a
usable dynamics processor, a sidechain ducker, and a small playground for custom
compression shapes.

![Formula Compressor interface](Docs/ui-level.png)

## Download

Windows x64 builds are published on the
[Releases](https://github.com/Nejiwwkr/formula-compressor/releases/latest) page
as a zip with the VST3, the standalone application, install notes, and a
SHA-256 checksum. The binaries are not code-signed, so Windows SmartScreen may
warn about an unknown publisher.

## How it works

```text
Main input ── input trim ── x detector ─┐
                                        ├── f(x, s) ── gain ── mix / makeup ── monitor ── out
SC1..SC4 ── selected ── HPF ── s detector ─┘
```

- `x` is the main-input detector level.
- `s` is the selected external sidechain detector level (Off / SC1–SC4).
- Both use the same Peak/RMS mode and Attack/Release/Hold; the sidechain has its
  own HPF.
- `x` and `s` use the mode selected next to the formula box:

```text
AMP    x and s are linear amplitudes; f(x,s) returns linear amplitude
       gain = f(x,s) / x

dBFS   x and s are dBFS; f(x,s) returns dBFS
       gain dB = f(x,s) - x
```

The formula describes the *static* transfer, while the time constants describe
how fast the detectors reach it. It is not per-sample waveshaping, so there is
no aliasing and no added latency.

The formula is evaluated at the current sidechain level: the curve view shows
the `f(x, s)` slice for the current `s`, and the `S` readout next to the formula
shows that level. With no sidechain, `s = 0` in AMP mode and `s = -120` in dBFS
mode.

In AMP mode the sign of `f(x,s)` carries polarity: a negative output means
**negative gain**, so both channels are inverted (drawn as a dashed curve);
`f(x,s) = 0` is true silence. In dBFS mode values are magnitudes, so
`f(x,s) = 0` means 0 dB (unity), not silence; use a very low value such as
`-120` for mute. The gain magnitude is clamped to +24 dB in both modes. With Mix
below 100%, an inverted gain partially cancels the dry signal.

The default starts in **dBFS** mode with a classic 4:1 compressor (threshold
-12 dBFS); it does not use `s` until you select a sidechain or write it into the
formula:

```text
min(x, -12 + (x + 12) / 4)      // x: dBFS
```

The `x:` switch inside the formula header toggles both x and s between `dBFS`
and `AMP`; the default formula and mode travel with the project state.

### Sidechain input and HPF

The SIDECHAIN card selects **Off / SC1 / SC2 / SC3 / SC4**. Each is an optional
stereo aux input; in FL Studio map the project sidechain slots to plugin inputs
3/4, 5/6, 7/8 and 9/10 on the wrapper's **Processing → VFX** page. The plugin
cannot read FL's track names or sidechain numbers; it only sees the aux buses.
Hosts that expose only one sidechain input can use SC1.

`SC HPF` is a 12 dB/oct high-pass on the sidechain detector only. It does not
change the main audio or the main `x` detector. With no sidechain selected, or
when the selected bus is not enabled, `s` is silent and the formula still runs.

### Stereo LINK

`LINK` is 0–100%. At 100% both channels share one linked detector (the default,
and the 1.2 behavior); at 0% each channel has its own detector and gain. The
GR meter and time view show the most-compressed channel. The sidechain detector
`s` itself is always linked.

### Monitoring

`MONITOR` switches the output to:

- **Off** — normal compressor output.
- **Delta** — `mix * (dry - dry * formulaGain)`, without make-up gain; this is
  the part the formula removes.
- **Sidechain** — the selected sidechain after its HPF (silent when no sidechain
  is available).

Bypass always wins and outputs the dry signal.

## Three views

The curve panel switches between three equal-scale views:

| View | Axes | What it shows |
|---|---|---|
| **LEVEL** | input / output level, dB | the usual compressor transfer curve; `f(x,s)=x` is the 45° diagonal |
| **GAIN** | input level dB / gain dB | `f(x,s)/x`; a constant gain is a flat line (`0.2x` = -14 dB), compression slopes down |
| **LINEAR** | linear amplitude / linear amplitude | ordinary coordinates; the slope is the gain factor |

The curve is the current `s` slice of `f(x, s)`. If the formula does not use `s`,
it stays static; otherwise it moves with the sidechain level and the `S` readout
shows the value used for the slice.

The region between the curve and unity uses two colours: **teal** for boost
(gain > 1) and **vermilion** for reduction (0 ≤ gain < 1). Negative output
(polarity inversion, AMP mode only) is shown with a **dashed curve** instead of
a third colour.

Screens: [LEVEL](Docs/ui-level.png) · [GAIN](Docs/ui-gain.png) ·
[LINEAR](Docs/ui-linear.png) · [inverted](Docs/ui-inverted.png) ·
[dBFS](Docs/ui-dbfs.png)

## Time view

The right-hand panel is a rolling 3-second level scope on the **same dBFS axis**
as the transfer plot (grid lines line up vertically). It shows input and output
levels after the **formula gain only** (Mix and Makeup are not included) as
colour-filled bands:

- **graphite** — the common body below both levels
- **vermilion** — input-only, i.e. removed by the formula
- **teal** — output-only, i.e. added by the formula
- **cobalt** — the gain curve (`f(x,s)/x` in dB) over the same window

There are no outline strokes around the level areas; the fills alone carry the
meaning. **Click the time panel to freeze** the transfer, time and level views
together; click again to resume. The time view always shows the compressor's
formula gain, not the Monitor signal.

## Highlights

- **Editable f(x, s)** with live recompilation while you type.
- **x: AMP / dBFS** switch: write either linear-amplitude or dB compressor curves.
- **Four selectable sidechain inputs** (SC1–SC4), a detector-only SC HPF, and
  sidechain monitoring.
- **Stereo LINK 0–100%** — linked by default, dual mono at 0%.
- **LEVEL / GAIN / LINEAR views** on equal-scale axes, two-colour boost /
  reduction shading, dashed inversion and a live operating point.
- **Time scope** with input/output levels and the gain curve on one dBFS axis.
- **Attack, Release, Hold** on the detector envelopes.
- **Peak / RMS detection** (RMS uses a 10 ms one-pole time constant).
- **Input gain, Mix and Makeup** for drive, parallel compression and output trim.
- **Fourteen built-in formulas** (linear, dB and sidechain) plus user presets.
- **Input, output and gain-reduction meters.**
- **0 latency**, mono or stereo, 44.1–192 kHz.
- Every knob and switch is a real VST3 parameter that can be automated.

## Formula language

`x` is the main detector level and `s` is the selected sidechain detector level,
both interpreted as **linear amplitude** or **dBFS** depending on the `x:` switch
next to the editor. `P1`–`P4` are the four knobs at the right of the knob row,
range -1..+1; use them to make a formula playable instead of editing constants.
The parser is case-insensitive, ignores whitespace, and accepts an optional
`f(x) = `, `f(x,s) = ` or `y = ` prefix.

```text
operators   + - * / % ^        ^ is power, right associative
compare     < <= > >= == !=    yield 0 or 1
logic       && || !            cond ? a : b
variables   x, s, P1..P4, pi, e
functions   abs min max clamp(v,lo,hi) pow(a,b) sqrt cbrt
            exp log/ln log10 log2 floor ceil round trunc sign
            sin cos tan asin acos atan atan2 sinh cosh tanh
            step(edge,x) smoothstep(e0,e1,x) mix(a,b,t)
            db(v) lin(d) rand() if(c,a,b)
```

`db(v)` is `20*log10(|v|)` and `lin(d)` is `10^(d/20)`, so curves can be written
in either domain. `rand()` returns a uniform random value in `[0,1)` on every
evaluation (useful for experiments, not for stable curves).

When no sidechain is selected, `s` is 0 in AMP and -120 in dBFS. In dBFS mode
`s` is negative, so a duck is written with a threshold, for example
`x - max(0, s + 60) / 4`; `x - s/10` would boost the signal when the sidechain
is quiet.

Examples (AMP mode):

```text
min(x, P1 + (x - P1) * P2)                  classic downward compression
lin(min(db(x), -12 + (db(x) + 12) / 4))     the same curve written in dB
x < P1 ? x : P1 + (x - P1) * P2             hard-knee variant
tanh(x * (1 + P1 * 4)) / tanh(1 + P1 * 4)   saturating curve
x < P1 ? x * P2 : x                         downward expander / gate
min(x, P1)                                  brick-wall limiter curve
-0.5 * x                                    -6 dB with polarity inversion
x * (1 - 0.8 * min(s, 1))                   sidechain duck using s
x - s / 10                                  subtle sidechain duck
```

The same curves in dBFS mode (x, s and f are dB values):

```text
min(x, -12 + (x + 12) / 4)                  classic 4:1
x - max(0, s + 60) / 4                      sidechain duck above -60 dBFS
x - max(0, s + 40)                          sidechain 1:1 duck above -40 dBFS
x < -30 ? x : mix(x, -24 + (x + 24) / 4, smoothstep(-30, -24, x))
x < -36 ? x + 12 : x                        upward compression
x < -40 ? -120 : x                          gate / mute below -40 dBFS
```

The applied gain magnitude is clamped to +24 dB; in AMP mode negative values are
inverted and zero is silence. Non-finite formula values are drawn as red vertical
lines and skipped by the engine. If the expression has a syntax error, the
previous valid curve keeps running while the status line shows the column and
the error.

## Parameters

| Control | Range | Default | Notes |
|---|---|---|---|
| Attack | 0.1–200 ms | 10 ms | envelope rise |
| Release | 5–2000 ms | 150 ms | envelope fall |
| Hold | 0–500 ms | 0 ms | freeze reduction before release |
| Mix | 0–100% | 100% | dry / wet |
| Input Gain | -24..+24 dB | 0 dB | trim before the main detector; external sidechain is not trimmed |
| Makeup | -12..+24 dB | 0 dB | output gain |
| Detector | Peak / RMS | Peak | applies to x and s; RMS uses a 10 ms one-pole time constant |
| SC | Off / SC1–SC4 | Off | selected aux sidechain |
| SC HPF | 0–500 Hz | 0 (Off) | 12 dB/oct high-pass on the sidechain detector only |
| Stereo Link | 0–100% | 100% | 100% linked, 0% dual mono |
| Monitor | Off / Delta / Sidechain | Off | Delta excludes make-up gain |
| P1–P4 | -1..+1 | 0.25, 0.25, 0, 0 | formula variables |
| Bypass | on / off | off | host-visible bypass parameter |

The formula text and its `x` mode are stored in the plug-in state, not as
automatable parameters; the SC, HPF, LINK and MONITOR controls are normal VST3
parameters.

## Presets

Built-in formulas (linear): *Classic 4:1*, *Soft Knee*, *Hard Limit*,
*Upward Compress*, *Expander / Gate*, *Tanh Comp*, *Log Comp*, *Two Stage*.
Built-in formulas (dBFS): *Classic 4:1 (dB)*, *Soft Knee (dB)*,
*Upward Compress (dB)*, *Gate (dB)*. Sidechain examples: *Sidechain Duck (dB)*
and *Sidechain Duck (AMP)*. Each preset restores its `x` mode; the SC input,
HPF, LINK and MONITOR controls are left as they are, so you can load a formula
without losing your routing.

User presets are full plug-in states (formula plus every control) stored in:

```text
%APPDATA%\FormulaCompressor\Presets
```

Use the preset list in the header to load one, or **SAVE** to store the current
state under a name.

## Build (Windows)

Requirements: Visual Studio 2022 with the Desktop C++ workload, CMake 3.24+.

JUCE 8.0.15 is fetched automatically by CMake from a pinned release tarball.
To build offline, point `FORMULA_JUCE_PATH` at a local JUCE checkout, or keep
the optional `ThirdParty/JUCE-8.0.15` directory in place:

```powershell
cmake -S . -B Build -G "Visual Studio 17 2022" -A x64 -DFORMULA_JUCE_PATH=C:\path\to\JUCE
```

Configure and build:

```powershell
cmake -S . -B Build -G "Visual Studio 17 2022" -A x64
cmake --build Build --config Release --target FormulaCompressor_VST3 FormulaCompressor_Standalone FormulaCompressorTests --parallel
```

Artefacts:

```text
Build\FormulaCompressor_artefacts\Release\VST3\Formula Compressor.vst3
Build\FormulaCompressor_artefacts\Release\Standalone\Formula Compressor.exe
```

## Tests

The console test app drives the real parser, engine and AudioProcessor offline:

```powershell
.\Build\FormulaCompressorTests_artefacts\Release\FormulaCompressorTests.exe
```

It covers expression precedence/functions/errors and limits, `s`, AMP and dBFS
domains, detector and attack/release/hold timing, stereo LINK, sidechain
routing and HPF, Monitor modes, mix/makeup/bypass, scope capture, extreme
formulas, state and preset round trips, and multiple sample rates and block sizes.

`FormulaCompressorTests.exe --screenshot Docs/ui-level.png level` renders the
editor without a DAW; the optional arguments are
`<level|gain|linear> ["formula"] [amp|db] [--size 900x680]`.

Validate the VST3 with pluginval:

```powershell
powershell -ExecutionPolicy Bypass -File Tools\run_pluginval.ps1 -SkipGuiTests
```

## Install

VST3 builds are not code-signed, so Windows SmartScreen may warn about an
unknown publisher.

```powershell
powershell -ExecutionPolicy Bypass -File Tools\install_vst3.ps1
```

Installs to `%APPDATA%\VST3`; add `-System` for
`C:\Program Files\Common Files\VST3`. Rescan plug-ins in your DAW afterwards.

## License

Formula Compressor is licensed under the GNU Affero General Public License v3.0
(AGPL-3.0). This is required because the project uses JUCE 8 under its AGPLv3
option; no commercial JUCE license is included. The corresponding source for
each release is this repository at the matching tag. See
[THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md) for JUCE and bundled dependency
notices, and [LICENSE](LICENSE) for the full license text.

## Limitations

- Sidechain is one selected aux input at a time; there is no multi-source sum
  inside the plug-in (sum the sends in the host if needed).
- There is no separate `x = sidechain` detector mode; classic sidechain
  compression is written as a formula using `s`.
- No lookahead: the reported latency is 0 and the transient response is set only
  by Attack.
- Make-up gain is manual; there is no auto-makeup that follows P1–P4.
- The formula drives a level-detector gain, not a per-sample waveshaper.
