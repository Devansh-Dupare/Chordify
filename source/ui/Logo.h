#pragma once

#include "Pt1Style.h"

// Chordify's logo, drawn in code so there is one resolution-independent source. The editor draws
// it live; the MakeArt tool (harness/MakeArt.cpp) exports every icon and installer image from it.
//
// The mark is a strip of PT-1 mini keys with a C major chord (C, E, G) lit in the accent colour,
// on the cream case, in the same light and palette as the plugin's skin.
namespace logo
{
    // Five white keys (C..G) with their black keys, C, E and G lit
    void drawKeysMark (juce::Graphics&, juce::Rectangle<float> area);

    // Square app icon on Apple's icon grid. `simplified` drops the fine detail (speaker grille,
    // printed name) that turns to noise at 16-48 px.
    void drawAppIcon (juce::Graphics&, juce::Rectangle<float> canvas, bool simplified);

    // Keys mark + "Chordify" + a small printed subtitle, as printed on the case. `ink` is the text
    // colour, so the same lockup works on light and dark backgrounds.
    void drawWordmark (juce::Graphics&, juce::Rectangle<float> area, juce::Colour ink = pt1::colours::ink);
}
