#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Casio PT-1-inspired skin: palette, the single light source every control shares, and fonts.
// Scoped to Chordify's editor only.
namespace pt1
{
    namespace colours
    {
        // Cream plastic case
        inline const juce::Colour caseTop { 0xfff4efe6 };
        inline const juce::Colour caseBottom { 0xffe6ddcd };
        inline const juce::Colour caseShade { 0xffd9cfbd }; // knob skirts, seams, button bodies
        inline const juce::Colour caseEdge { 0xffc2b6a1 };

        // Silkscreened print
        inline const juce::Colour ink { 0xff4a453e };
        inline const juce::Colour inkDim { 0xff8e867a };

        // The one accent: selected keys, the playing slot's LED, knob indicators
        inline const juce::Colour accent { 0xffe0552b };
        inline const juce::Colour accentDark { 0xffa63d1d };

        // Keys
        inline const juce::Colour ivoryTop { 0xfffffdf8 };
        inline const juce::Colour ivoryBottom { 0xffece6da };
        inline const juce::Colour ivoryLip { 0xffd8cfbf };
        inline const juce::Colour ebonyTop { 0xff3b3835 };
        inline const juce::Colour ebonyBottom { 0xff1d1c1b };
        inline const juce::Colour ebonyLip { 0xff4a4642 };
        inline const juce::Colour keybed { 0xff2a2825 };

        // Knob caps: charcoal plastic
        inline const juce::Colour capLight { 0xff64605b };
        inline const juce::Colour capDark { 0xff1e1d1c };

        // LCD
        inline const juce::Colour lcdTop { 0xffaab196 };
        inline const juce::Colour lcdBottom { 0xff959d82 };
        inline const juce::Colour lcdInk { 0xff252920 };
        inline const juce::Colour bezel { 0xff3a3835 };
    }

    // One light source for every knob, key and button: above and to the left. Shadows fall down
    // and to the right, highlights sit on the upper-left edges.
    namespace light
    {
        inline const juce::Point<float> towardsLight { -0.6f, -0.8f }; // unit vector
        inline constexpr float highlightAngle = -0.6435f;              // radians from 12 o'clock, pointing at the light

        inline const juce::Colour shadow = juce::Colours::black;
        inline constexpr int shadowRadius = 8;       // blur radius at 1x
        inline const juce::Point<int> shadowOffset { 2, 3 };
        inline constexpr float shadowAlpha = 0.30f;
    }

    // Plain rounded sans for printed labels (macOS ships Arial Rounded MT Bold; elsewhere JUCE
    // falls back to the default sans)
    inline juce::FontOptions printFont (float size)
    {
        return juce::FontOptions ("Arial Rounded MT Bold", size, juce::Font::plain);
    }

    inline juce::FontOptions textFont (float size, bool bold = false)
    {
        return juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain);
    }

    // Scale of physical pixels to logical units for whatever this Graphics draws into (display
    // scale x editor zoom), so cached images are rendered at full resolution
    inline float physicalScale (juce::Graphics& g)
    {
        return std::max (1.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
    }

    // Renders into an image at the given physical scale and returns it; draw it back with
    // g.drawImage (image, logicalBounds)
    template <typename Painter>
    juce::Image renderCached (juce::Rectangle<float> logicalBounds, float scale, Painter&& paint)
    {
        const auto w = std::max (1, juce::roundToInt (logicalBounds.getWidth() * scale));
        const auto h = std::max (1, juce::roundToInt (logicalBounds.getHeight() * scale));
        juce::Image image (juce::Image::ARGB, w, h, true);
        juce::Graphics g (image);
        g.addTransform (juce::AffineTransform::scale (scale).translated (-logicalBounds.getX() * scale, -logicalBounds.getY() * scale));
        paint (g);
        return image;
    }
}
