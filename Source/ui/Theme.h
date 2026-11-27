#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace formula::theme
{

// Drafting paper, ink and signal colours: warm paper with a blue/red graph accent.
inline const juce::Colour paper       { 0xfff2efe6 };
inline const juce::Colour paperBright { 0xfffbf8f0 };
inline const juce::Colour panel       { 0xffeae4d6 };
inline const juce::Colour panelAlt    { 0xffded7c5 };
inline const juce::Colour border      { 0xffc8c0ac };
inline const juce::Colour ink         { 0xff1b2430 };
inline const juce::Colour text        { 0xff2c3742 };
inline const juce::Colour secondary   { 0xff68746f };
inline const juce::Colour muted       { 0xff98a29b };
inline const juce::Colour grid        { 0xffded8c7 };
inline const juce::Colour gridStrong  { 0xffb8af98 };

inline const juce::Colour cobalt      { 0xff1d5fbf };
inline const juce::Colour vermilion   { 0xffcf4426 };
inline const juce::Colour teal        { 0xff117a67 };
inline const juce::Colour amber       { 0xffb26a12 };

} // namespace formula::theme

namespace formula::type
{

inline constexpr float display    = 26.0f;
inline constexpr float primary    = 13.0f;
inline constexpr float control    = 12.0f;
inline constexpr float annotation = 11.0f;
inline constexpr float mono       = 15.0f;

} // namespace formula::type
