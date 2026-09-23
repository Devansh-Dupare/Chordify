#include "Logo.h"

namespace logo
{
    namespace
    {
        using namespace pt1;

        constexpr int numWhiteKeys = 5;                  // C D E F G
        constexpr bool litWhite[numWhiteKeys] = { true, false, true, false, true };
        constexpr int blackAfterWhite[] = { 0, 1, 3 };  // C#, D#, F#

        juce::Path roundedBottom (juce::Rectangle<float> r, float corner)
        {
            juce::Path p;
            p.addRoundedRectangle (r.getX(), r.getY(), r.getWidth(), r.getHeight(), corner, corner, false, false, true, true);
            return p;
        }

        juce::Path rounded (juce::Rectangle<float> r, float corner)
        {
            juce::Path p;
            p.addRoundedRectangle (r, corner);
            return p;
        }
    }

    void drawKeysMark (juce::Graphics& g, juce::Rectangle<float> area)
    {
        const auto unit = area.getHeight();
        const auto bedCorner = unit * 0.07f;

        // Keybed
        g.setColour (colours::keybed);
        g.fillPath (rounded (area, bedCorner));

        const auto keys = area.reduced (unit * 0.025f);
        const auto whiteWidth = keys.getWidth() / (float) numWhiteKeys;
        const auto gap = std::max (0.5f, whiteWidth * 0.045f);
        const auto lip = keys.getHeight() * 0.07f;
        const auto corner = whiteWidth * 0.12f;

        for (int i = 0; i < numWhiteKeys; ++i)
        {
            const auto key = keys.withX (keys.getX() + (float) i * whiteWidth).withWidth (whiteWidth).reduced (gap / 2.0f, 0.0f);
            const auto face = key.withTrimmedBottom (lip);
            const auto lit = litWhite[i];

            g.setColour (lit ? colours::accentDark : colours::ivoryLip);
            g.fillPath (roundedBottom (key, corner));
            g.setGradientFill (juce::ColourGradient::vertical (lit ? colours::accent.brighter (0.25f) : colours::ivoryTop, face.getY(),
                lit ? colours::accent : colours::ivoryBottom, face.getBottom()));
            g.fillPath (roundedBottom (face, corner * 0.8f));
        }

        const auto blackWidth = whiteWidth * 0.58f;
        const auto blackHeight = keys.getHeight() * 0.6f;
        for (auto white : blackAfterWhite)
        {
            const auto key = juce::Rectangle<float> (keys.getX() + (float) (white + 1) * whiteWidth - blackWidth / 2.0f, keys.getY(), blackWidth, blackHeight);
            juce::DropShadow (light::shadow.withAlpha (0.45f), juce::roundToInt (std::max (1.0f, unit * 0.04f)),
                { juce::roundToInt (unit * 0.012f), juce::roundToInt (unit * 0.025f) })
                .drawForPath (g, roundedBottom (key, corner));
            g.setColour (colours::ebonyLip);
            g.fillPath (roundedBottom (key, corner));
            const auto face = key.withTrimmedBottom (lip * 0.8f).reduced (blackWidth * 0.08f, 0.0f);
            g.setGradientFill (juce::ColourGradient::vertical (colours::ebonyTop, face.getY(), colours::ebonyBottom, face.getBottom()));
            g.fillPath (roundedBottom (face, corner * 0.8f));

            // Gloss on the side facing the light
            const auto gloss = face.withWidth (face.getWidth() * 0.32f).translated (face.getWidth() * 0.14f, 0.0f).withTrimmedBottom (face.getHeight() * 0.35f);
            g.setGradientFill (juce::ColourGradient::vertical (juce::Colours::white.withAlpha (0.2f), gloss.getY(), juce::Colours::transparentWhite, gloss.getBottom()));
            g.fillRoundedRectangle (gloss, gloss.getWidth() / 2.0f);
        }

        // The case overhangs the back of the keys
        g.setGradientFill (juce::ColourGradient::vertical (juce::Colours::black.withAlpha (0.35f), area.getY(), juce::Colours::transparentBlack, area.getY() + unit * 0.1f));
        g.fillRect (area.withHeight (unit * 0.1f).reduced (bedCorner, 0.0f));
    }

    void drawAppIcon (juce::Graphics& g, juce::Rectangle<float> canvas, bool simplified)
    {
        // Apple's icon grid: an 824/1024 body with a ~22.5% corner radius, leaving room for the shadow
        const auto size = std::min (canvas.getWidth(), canvas.getHeight());
        const auto body = juce::Rectangle<float> (size, size).withCentre (canvas.getCentre()).reduced (size * (simplified ? 0.06f : 0.098f));
        const auto w = body.getWidth();
        const auto corner = w * 0.225f;

        juce::DropShadow (light::shadow.withAlpha (0.3f), juce::roundToInt (std::max (1.0f, w * 0.03f)), { 0, juce::roundToInt (w * 0.015f) })
            .drawForPath (g, rounded (body, corner));

        // Cream case, a little brighter at the top, with a highlight on the upper edge
        g.setGradientFill (juce::ColourGradient::vertical (colours::caseTop.brighter (0.02f), body.getY(), colours::caseBottom, body.getBottom()));
        g.fillPath (rounded (body, corner));
        g.setColour (juce::Colours::white.withAlpha (0.7f));
        g.strokePath (rounded (body.reduced (w * 0.012f), corner * 0.95f), juce::PathStrokeType (std::max (1.0f, w * 0.006f)));
        g.setColour (colours::caseEdge);
        g.strokePath (rounded (body, corner), juce::PathStrokeType (std::max (0.75f, w * 0.004f)));

        // Power LED, top right: lit in the accent colour
        const auto led = juce::Rectangle<float> (w * 0.075f, w * 0.075f).withCentre ({ body.getRight() - w * 0.2f, body.getY() + w * 0.2f });
        juce::DropShadow (colours::accent.withAlpha (0.7f), juce::roundToInt (std::max (1.0f, w * 0.03f)), {}).drawForPath (g, [&] { juce::Path p; p.addEllipse (led); return p; }());
        g.setColour (colours::accent);
        g.fillEllipse (led);
        g.setColour (juce::Colours::white.withAlpha (0.55f));
        g.fillEllipse (led.reduced (led.getWidth() * 0.3f).translated (-led.getWidth() * 0.1f, -led.getWidth() * 0.1f));

        if (! simplified)
        {
            // Speaker grille: the PT-1's rows of holes
            for (int row = 0; row < 4; ++row)
                for (int col = 0; col < 8; ++col)
                {
                    const auto hole = juce::Rectangle<float> (w * 0.036f, w * 0.036f)
                                          .withCentre ({ body.getX() + w * (0.17f + 0.058f * (float) col), body.getY() + w * (0.15f + 0.058f * (float) row) });
                    g.setColour (juce::Colours::white.withAlpha (0.8f));
                    g.fillEllipse (hole.translated (0.0f, w * 0.004f));
                    g.setColour (colours::ink.withAlpha (0.75f));
                    g.fillEllipse (hole);
                }

            // Printed model name
            g.setColour (colours::ink);
            g.setFont (printFont (w * 0.085f));
            g.drawText ("Chordify", juce::Rectangle<float> (body.getX() + w * 0.12f, body.getY() + w * 0.38f, w * 0.76f, w * 0.12f),
                juce::Justification::centredLeft, false);
        }

        // The keys, along the bottom as on the real instrument
        const auto keysTop = simplified ? 0.34f : 0.53f;
        drawKeysMark (g, juce::Rectangle<float> (body.getX() + w * 0.1f, body.getY() + w * keysTop, w * 0.8f, w * (0.88f - keysTop)));
    }

    void drawWordmark (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour ink)
    {
        const auto h = area.getHeight();
        drawKeysMark (g, area.removeFromLeft (h * 1.35f).withSizeKeepingCentre (h * 1.35f, h * 0.78f));
        area.removeFromLeft (h * 0.22f);

        g.setColour (ink);
        g.setFont (printFont (h * 0.5f));
        g.drawText ("Chordify", area.removeFromTop (h * 0.66f), juce::Justification::bottomLeft, false);
        g.setColour (ink.withAlpha (0.65f));
        g.setFont (printFont (h * 0.155f));
        g.drawText ("CHORD RESONATOR  " + juce::String::fromUTF8 ("\xc2\xb7") + "  DUPHON", area.withTrimmedTop (h * 0.04f),
            juce::Justification::topLeft, false);
    }
}
