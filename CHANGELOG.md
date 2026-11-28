# Changelog

## 1.4.0 — 2026-11-28

- Multi-sidechain modulation: the formula is now `f(x, s1..s4, s)`. Each of the
  four aux buses has its own detector (`s1`..`s4`); `s` is their linear sum
  converted to the current domain, so two -6 dBFS sidechains sum to 0 dBFS.
- The SIDECHAIN selector is now **Off / On**. On uses every aux bus the host
  enables; there is no bus-number choice. With one aux bus the formula reduces
  to `s1` and `s2`..`s4` stay silent.
- The four sidechains share the SC HPF frequency but keep independent filter
  and envelope state. Monitor **Sidechain** now plays the sum of the four
  post-HPF buses (phase cancellation and levels above 0 dBFS are possible).
- Adaptive formula header: Off shows `f(x:`, On shows `f(x, s1..4, s:` and the
  `S1..S4 + S` readout; the editor widens when the sidechain is off. Fixed the
  `) =` label wrapping to two lines / eliding into ellipses at 900 px.
- Parser accepts any balanced `f(...)` / `y =` prefix, adds `s1`..`s4`, and
  reports `s5` and other unknown variables as errors. Help text documents the
  per-bus variables and the linear-sum rule.
- Formula curves rebuild when any `sN` or the total changes, not only the total.
- State version 5. `scEnabled` (bool) replaces the `scInput` choice. Loading a
  v4 state maps `scInput` 0 to Off and 1–4 to On and removes the old child.
  The selected bus identity is not restored: On feeds all enabled aux buses.
  Formulas using `s` now follow the sum instead of the selected bus; projects
  with one routed bus are unchanged, multi-bus projects should switch to `sN`.
  Old `scInput` automation is not migrated; automate `scEnabled` instead.
- Tests cover per-bus formulas and sentinels, the dB total, independent hold and
  HPF clearing, Monitor sum, OFF/absent-bus reset, state migration, and the
  Off/On header layout at 1000×720 and 900×680. The screenshot tool gains
  `--sidechain`; CI renders both states at both sizes.

## 1.3.0 — 2026-11-27

- New two-variable formula `f(x, s)`: `x` is the main-input detector level and
  `s` is the selected external sidechain detector level. Both use the current
  Peak/RMS mode and Attack/Release/Hold timing; s is rendered in the same AMP /
  dBFS domain as x. With no sidechain, s is 0 (AMP) or -120 (dBFS).
- Four optional stereo aux sidechain inputs SC1–SC4 with an in-plugin selector.
  FL Studio maps its sidechain slots to plugin inputs 3/4, 5/6, 7/8 and 9/10 on
  the VFX page. The selected bus is fallback-safe when unavailable.
- Sidechain HPF (0 = off, 20–500 Hz, 12 dB/oct) applies only to the sidechain
  detector. New Monitor modes: Off, Delta (removed signal, no make-up), and
  Sidechain listen (post-HPF).
- Full stereo LINK 0–100%: 100% is the previous fully-linked behavior, 0% gives
  each channel its own detector and gain. Sidechain s remains linked.
- The transfer curve shows the current-s slice of `f(x, s)`, with an S readout;
  it rebuilds as the sidechain changes only when the formula uses s.
- New SIDECHAIN card with HPF/LINK knobs and an SC selector; MONITOR is in the
  GAIN header. The window stays 1000×720 with a 900×680 minimum, and CI renders
  both sizes.
- State version 4. New parameters: `scInput`, `scHpf`, `stereoLink`, `monitor`.
  Loading a v3 state explicitly restores their defaults instead of keeping the
  current instance values.
- Added Sidechain Duck (dB) and Sidechain Duck (AMP) built-in formulas; the
  formula help documents s, the no-sidechain sentinel, and dB-domain examples.
- Still no lookahead (0 latency) and no auto-makeup; MAKEUP remains manual.
- Tests cover s evaluation, LINK 0/50/100, sidechain routing/HPF, Monitor,
  multi-aux selection, state migration and 900×680 UI rendering.

## 1.2.1 — 2026-11-27

- Relicensed to AGPL-3.0 to match the JUCE 8 AGPLv3 distribution option.
  Release zips now include `LICENSE` and `THIRD-PARTY-NOTICES.md`, and the
  release workflow publishes a SHA-256 checksum.
- CI is repaired for the current runner images: the Windows job uses
  `windows-2022` with VS 2022, and JUCE is fetched by CMake from a pinned
  8.0.15 tarball (with a local `FORMULA_JUCE_PATH` escape hatch). Fresh clones
  no longer need a manually extracted `ThirdParty/` directory.
- Release automation now builds on a read-only token and publishes from a
  separate tag-only job with `contents: write`; tag/version consistency and the
  zip checksum are verified before upload. pluginval's unsupported
  `--junit-xml` argument was removed in favour of a log file.
- Fixed a message-thread / audio-thread race: state restores now request
  `engine.reset()` through an atomic consumed at the start of `processBlock()`
  instead of resetting the engine directly from `setStateInformation()`.
- Program versions are retained for the processor's lifetime, so the audio
  thread's raw program pointer cannot dangle inside a process call.
- The GR meter now reports the largest reduction in the block (minimum gain)
  instead of only the last sample, so short transients are no longer missed.
- NaN/Inf input is sanitised, detector input is clamped to prevent square
  overflow, output samples have a final finite guard, and the engine has its
  own denormal guard.
- State loading now reads `stateVersion` and explicitly treats pre-1.2 states
  without `xDomain` as AMP; unknown future versions load best-effort.
- Documentation: RMS is described as a 10 ms one-pole mean-square time constant
  rather than a 10 ms window; the automation claim no longer implies plug-in
  level undo. Tests cover block-peak GR, NaN/Inf/extreme input, RMS time
  constant, state migration, mono processing and sample-rate changes.
- No state-format change: `stateVersion` remains 3.

## 1.2.0

- New `x:` AMP / dBFS switch. In dBFS mode both `x` and `f(x)` are in dBFS and
  `gain dB = f(x) - x`, so formulas can be written like a traditional
  compressor. The mode is saved in the project state and in presets.
- Built-in presets now declare their x domain; merged dB variants added
  (Classic 4:1, Soft Knee, Upward Compress, Gate).
- Simplified curve colours: only two area fills (teal boost, vermilion
  reduction); negative output is shown as a dashed cobalt curve. Violet and the
  legend/readout/how-to panel were removed.
- Replaced the right-hand info panel with a rolling 3-second level scope on the
  same dBFS axis as the transfer plot: input and output peak levels (formula
  gain only, no Mix/Makeup) plus the gain curve in one graph, with graphite
  common / vermilion removed / teal added regions.
- Scope data is captured lock-free on the audio thread; the editor resets it when
  reopened. Added scope and dB-domain tests.
- Added an INPUT gain knob (pre-detector trim) in the new GAIN card together
  with MIX and MAKEUP.
- UI grouping: controls are arranged in TIME / GAIN / FORMULA cards; the
  detector is now PEAK/RMS buttons in the LEVELS header, and the LEVELS panel is
  wider. Segmented button groups (x domain, view, detector) stay visually
  distinct.
  Header preset/save controls are grouped separately from bypass; meter labels
  and values are aligned with the other panels, and the signal colours are
  unified across scope and meter (grey input, graphite output, cobalt GR).
- Panel titles sit above their plots with consistent inner padding; the transfer,
  time and meter panels share the same dB grid and vertical alignment. The light
  panel is drawn only inside the plot axes; tick labels sit on the card outside
  the axes, so 12 / -60 no longer cross the border.
- Header controls now align to each plot's right edge (the LINEAR view button,
  the 3 s label and the PEAK/RMS buttons). The plot right margin is reduced and
  the freed width goes to the TIME axis.
- Card spacing is uniform: all dark blocks use 10 px gaps (formula to plots,
  plots to knob cards, and between cards in each row), with the image cards
  filling their bounds. Their reclaimed horizontal space widens the TIME axis.
- Levels use the same -60..+12 dB axis for IN, OUT and GR (GR no longer has a
  separate, changing full scale).
- Clearing the formula field keeps it empty; the default formula is no longer
  re-inserted while typing.
- Clicking the TIME plot freezes the transfer, time and level views together
  (cached snapshot; click again to resume). The level areas no longer draw
  outline strokes; the three fill colours carry the reading.
- New default: `x` starts in **dBFS** mode with
  `min(x, -12 + (x + 12) / 4)`. The formula header now reads
  `f(x: dBFS | AMP) = ...` with the domain switch inline, and HELP moved to the
  second line, right-aligned.
- State version 3 (older states without `xDomain` still load as AMP).

## 1.1.0

- New equal-scale curve views with a LEVEL / GAIN / LINEAR switch:
  - LEVEL: input and output level in dB, square plot, unity is a 45 degree line.
  - GAIN: gain = f(x)/x in dB; a constant gain is a flat line (0.2x = -14 dB).
  - LINEAR: linear amplitude on ordinary axes; the slope is the gain factor.
- The area between the curve and unity is colour-coded: teal for boost,
  vermilion for reduction and violet for polarity inversion.
- The side panel adds a live/hover readout (linear + dB), a legend and a
  per-view "how to read" note.
- Negative gain now carries polarity instead of being clamped to silence:
  f(x) < 0 inverts both channels (INV), f(x) = 0 is true silence. The gain
  magnitude is clamped to +24 dB.
- GR meter and readout show an INV indicator for negative gain.
- Help text, README and tests updated; no state format change.

## 1.0.0

Initial release.

- User-editable transfer function `f(x)` with live compile and error reporting.
- Stack-bytecode expression engine: arithmetic, power, comparisons, logic,
  ternary, a broad elementary function set, `db()`, `lin()`, `rand()`, and the
  `P1`–`P4` knob variables.
- Peak / RMS detector with Attack, Release and Hold on the envelope.
- Mix and Makeup gain.
- Live transfer plot with gain-reduction shading, operating point and hover
  readout.
- Input / output / gain-reduction meters.
- Eight built-in formulas and XML user presets.
- VST3 and standalone builds, 0 latency, mono or stereo, 44.1–192 kHz.
- Offline console test suite and pluginval-compatible packaging scripts.
