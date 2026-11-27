# Changelog

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
