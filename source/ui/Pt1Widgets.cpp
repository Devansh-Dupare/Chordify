#include "Pt1Widgets.h"
#include "Pt1LookAndFeel.h"

namespace pt1
{
    namespace
    {
        constexpr int labelHeight = 16;
        constexpr float meterMinDb = -60.0f;
        constexpr int meterSegments = 24;

        void drawScrew (juce::Graphics& g, juce::Point<float> centre, float radius)
        {
            juce::Path head;
            head.addEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre));
            juce::DropShadow (light::shadow.withAlpha (0.25f), 3, { 1, 1 }).drawForPath (g, head);
            g.setGradientFill (juce::ColourGradient (colours::caseTop, centre + light::towardsLight * radius, colours::caseShade.darker (0.2f),
                centre - light::towardsLight * radius, false));
            g.fillPath (head);
            g.setColour (colours::caseEdge.darker (0.2f));
            g.strokePath (head, juce::PathStrokeType (0.8f));
            g.drawLine ({ centre.translated (-radius * 0.6f, radius * 0.25f), centre.translated (radius * 0.6f, -radius * 0.25f) }, 1.2f);
        }
    }

    //==============================================================================
    CaseBackground::CaseBackground()
    {
        setInterceptsMouseClicks (false, false);
        setOpaque (true);
    }

    void CaseBackground::setLayout (juce::Rectangle<int> brandArea, std::vector<PrintedGroup> newGroups, std::vector<int> seamYs, std::vector<PrintedText> printedText)
    {
        brand = brandArea;
        groups = std::move (newGroups);
        seams = std::move (seamYs);
        text = std::move (printedText);
        cache = {};
        repaint();
    }

    void CaseBackground::paint (juce::Graphics& g)
    {
        const auto scale = physicalScale (g);
        const auto size = getLocalBounds().toFloat();
        if (! cache.isValid() || ! juce::approximatelyEqual (scale, cacheScale)
            || cache.getWidth() != juce::roundToInt (size.getWidth() * scale))
        {
            cache = renderCached (size, scale, [this] (juce::Graphics& gi) { render (gi); });
            cacheScale = scale;
        }
        g.drawImage (cache, size);
    }

    void CaseBackground::render (juce::Graphics& g) const
    {
        const auto bounds = getLocalBounds().toFloat();

        // Light falling on molded plastic: a few percent brighter at the top
        g.setGradientFill (juce::ColourGradient::vertical (colours::caseTop, 0.0f, colours::caseBottom, bounds.getBottom()));
        g.fillRect (bounds);

        // Fine grain so the plastic doesn't look like a sterile digital fill
        juce::Random random (20260924);
        for (int i = 0; i < (int) (bounds.getWidth() * bounds.getHeight() / 9.0f); ++i)
        {
            const auto dark = random.nextBool();
            g.setColour ((dark ? juce::Colours::black : juce::Colours::white).withAlpha (0.018f + 0.02f * random.nextFloat()));
            g.fillRect (random.nextFloat() * bounds.getWidth(), random.nextFloat() * bounds.getHeight(), 1.0f, 1.0f);
        }

        // Seams: a shadowed groove with a highlight under it
        for (auto y : seams)
        {
            g.setColour (colours::caseEdge.withAlpha (0.8f));
            g.fillRect (0.0f, (float) y, bounds.getWidth(), 1.0f);
            g.setColour (juce::Colours::white.withAlpha (0.7f));
            g.fillRect (0.0f, (float) y + 1.0f, bounds.getWidth(), 1.0f);
        }

        // Outer edge of the case
        g.setColour (colours::caseEdge);
        g.drawRect (bounds, 1.0f);

        // Printed group titles with a thin bracket line, like silkscreen on the case
        for (const auto& group : groups)
        {
            const auto area = group.area.toFloat();
            const auto titleArea = juce::Rectangle<float> (area.getX(), area.getY() - 18.0f, area.getWidth(), 14.0f);
            g.setFont (printFont (11.0f));
            const auto titleWidth = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), group.title) + 14.0f;

            g.setColour (colours::ink.withAlpha (0.45f));
            const auto lineY = titleArea.getCentreY();
            const auto gapLeft = titleArea.getCentreX() - titleWidth / 2.0f;
            const auto gapRight = titleArea.getCentreX() + titleWidth / 2.0f;
            g.drawLine (area.getX(), lineY, gapLeft, lineY, 1.0f);
            g.drawLine (gapRight, lineY, area.getRight(), lineY, 1.0f);
            g.drawLine (area.getX(), lineY, area.getX(), lineY + 5.0f, 1.0f);
            g.drawLine (area.getRight(), lineY, area.getRight(), lineY + 5.0f, 1.0f);

            g.setColour (colours::ink);
            g.drawText (group.title, titleArea, juce::Justification::centred, false);
        }

        for (const auto& item : text)
        {
            g.setColour (item.strong ? colours::ink : colours::inkDim);
            g.setFont (printFont (item.strong ? 11.0f : 10.0f));
            g.drawText (item.text, item.area, juce::Justification::centredLeft, true);
        }

        // Model name, printed like the lettering on the real case rather than a logo lockup
        if (! brand.isEmpty())
        {
            auto area = brand.toFloat();
            g.setColour (colours::ink);
            g.setFont (printFont (30.0f));
            g.drawText ("Chordify", area.removeFromTop (area.getHeight() * 0.58f), juce::Justification::bottomLeft, false);
            g.setColour (colours::accent);
            g.fillRect (area.removeFromTop (5.0f).withTrimmedTop (2.0f).withWidth (132.0f));
            g.setColour (colours::inkDim);
            g.setFont (printFont (10.0f));
            g.drawText ("CHORD RESONATOR  " + juce::String::fromUTF8 ("\xc2\xb7") + "  DUPHON", area, juce::Justification::centredLeft, false);
        }

        // Faux screws in the corners
        constexpr float inset = 9.0f;
        for (auto corner : { bounds.getTopLeft(), bounds.getTopRight(), bounds.getBottomLeft(), bounds.getBottomRight() })
            drawScrew (g, { corner.x + (corner.x > 0.0f ? -inset : inset), corner.y + (corner.y > 0.0f ? -inset : inset) }, 4.0f);
    }

    //==============================================================================
    void DisplayWindow::setChord (const juce::String& notes, const juce::String& status)
    {
        if (notes == notesText && status == statusText)
            return;
        notesText = notes;
        statusText = status;
        repaint();
    }

    void DisplayWindow::setLevels (std::array<float, 2> peaks)
    {
        constexpr float fallDbPerFrame = 1.2f;
        auto changed = false;
        for (size_t ch = 0; ch < 2; ++ch)
        {
            const auto db = juce::jlimit (meterMinDb, 0.0f, juce::Decibels::gainToDecibels (peaks[ch], meterMinDb));
            const auto next = std::max (db, levelDb[ch] - fallDbPerFrame);
            changed = changed || std::abs (next - levelDb[ch]) > 0.01f;
            levelDb[ch] = next;
        }
        if (changed)
            repaint();
    }

    void DisplayWindow::flash (const juce::String& newMessage)
    {
        message = newMessage;
        startTimer (2500);
        repaint();
    }

    void DisplayWindow::timerCallback()
    {
        stopTimer();
        message.clear();
        repaint();
    }

    void DisplayWindow::paint (juce::Graphics& g)
    {
        const auto bounds = getLocalBounds().toFloat();

        // Dark plastic bezel, recessed into the case
        g.setColour (juce::Colours::white.withAlpha (0.7f)); // highlight on the lower lip of the recess
        g.fillRoundedRectangle (bounds.translated (0.0f, 1.0f), 7.0f);
        g.setGradientFill (juce::ColourGradient::vertical (colours::bezel.darker (0.3f), bounds.getY(), colours::bezel, bounds.getBottom()));
        g.fillRoundedRectangle (bounds, 7.0f);

        // LCD glass with an inner shadow along the top-left edges (the light comes from there)
        const auto lcd = bounds.reduced (7.0f);
        g.setGradientFill (juce::ColourGradient::vertical (colours::lcdTop, lcd.getY(), colours::lcdBottom, lcd.getBottom()));
        g.fillRoundedRectangle (lcd, 3.0f);
        g.setGradientFill (juce::ColourGradient::vertical (juce::Colours::black.withAlpha (0.28f), lcd.getY(), juce::Colours::transparentBlack, lcd.getY() + 8.0f));
        g.fillRoundedRectangle (lcd, 3.0f);
        g.setGradientFill (juce::ColourGradient::horizontal (juce::Colours::black.withAlpha (0.18f), lcd.getX(), juce::Colours::transparentBlack, lcd.getX() + 6.0f));
        g.fillRoundedRectangle (lcd, 3.0f);

        auto content = lcd.reduced (12.0f, 6.0f);

        // Level meter as LCD segments on the right; unlit segments ghost faintly like a real LCD
        auto meter = content.removeFromRight (120.0f);
        content.removeFromRight (12.0f);
        g.setFont (printFont (9.0f));
        for (size_t ch = 0; ch < 2; ++ch)
        {
            auto row = meter.withHeight (10.0f).withY (meter.getCentreY() - 12.0f + (float) ch * 14.0f);
            g.setColour (colours::lcdInk.withAlpha (0.8f));
            g.drawText (ch == 0 ? "L" : "R", row.removeFromLeft (10.0f), juce::Justification::centredLeft, false);
            const auto segmentWidth = row.getWidth() / (float) meterSegments;
            const auto lit = juce::roundToInt ((levelDb[ch] - meterMinDb) / -meterMinDb * (float) meterSegments);
            for (int i = 0; i < meterSegments; ++i)
            {
                g.setColour (colours::lcdInk.withAlpha (i < lit ? 0.85f : 0.07f));
                g.fillRect (row.getX() + (float) i * segmentWidth, row.getY(), segmentWidth - 1.5f, row.getHeight());
            }
        }

        // Chord notes, large; status or a message underneath
        g.setColour (colours::lcdInk);
        g.setFont (textFont (26.0f, true));
        g.drawFittedText (notesText, content.removeFromTop (content.getHeight() * 0.62f).toNearestInt(), juce::Justification::bottomLeft, 1, 0.7f);
        g.setFont (textFont (12.5f, true));
        g.setColour (colours::lcdInk.withAlpha (message.isNotEmpty() ? 1.0f : 0.75f));
        g.drawText ((message.isNotEmpty() ? message : statusText).toUpperCase(), content, juce::Justification::centredLeft, true);
    }

    //==============================================================================
    Knob::Knob (juce::AudioProcessorValueTreeState& tree, const juce::String& paramId, const juce::String& printedName)
        : name (printedName), attachment (tree, paramId, slider)
    {
        auto* param = tree.getParameter (paramId);

        // Show the unit in the value readout; typed values still parse ("1.5 s" and "1.5" both work)
        if (const auto unit = param->getLabel(); unit.isNotEmpty())
            slider.textFromValueFunction = [param, unit] (double value) {
                return param->getText (param->convertTo0to1 ((float) value), 32) + " " + unit;
            };
        slider.updateText();
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, preferredWidth, 16);
        slider.setMouseDragSensitivity (200);
        addAndMakeVisible (slider);
    }

    void Knob::resized()
    {
        // Knob square under the label, value readout right beneath it
        auto area = getLocalBounds().withTrimmedTop (labelHeight + 2);
        slider.setBounds (area.removeFromTop (std::min (area.getHeight(), getWidth() + 14)));
    }

    void Knob::paint (juce::Graphics& g)
    {
        // Printed straight on the case: flat colour, no effects
        g.setColour (colours::ink);
        g.setFont (printFont (10.5f));
        g.drawText (name.toUpperCase(), getLocalBounds().removeFromTop (labelHeight), juce::Justification::centred, false);
    }

    //==============================================================================
    RockerSwitch::RockerSwitch (juce::AudioProcessorValueTreeState& tree, const juce::String& paramId, const juce::String& printedName,
        juce::StringArray sideLabels)
        : name (printedName),
          labels (std::move (sideLabels)),
          attachment (*tree.getParameter (paramId), [this] (float value) { selected = juce::roundToInt (value); repaint(); })
    {
        attachment.sendInitialUpdate();
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    juce::Rectangle<float> RockerSwitch::switchBounds() const
    {
        return getLocalBounds().toFloat().withTrimmedTop ((float) labelHeight + 14.0f).withHeight (40.0f).reduced (6.0f, 0.0f);
    }

    void RockerSwitch::mouseDown (const juce::MouseEvent& event)
    {
        const auto side = event.position.x < switchBounds().getCentreX() ? 0 : 1;
        if (side != selected)
            attachment.setValueAsCompleteGesture ((float) side);
    }

    void RockerSwitch::paint (juce::Graphics& g)
    {
        g.setColour (colours::ink);
        g.setFont (printFont (10.5f));
        g.drawText (name.toUpperCase(), getLocalBounds().removeFromTop (labelHeight), juce::Justification::centred, false);

        auto body = switchBounds();

        // Recess in the case the rocker sits in
        g.setColour (colours::bezel.withAlpha (0.85f));
        g.fillRoundedRectangle (body.expanded (2.0f), 5.0f);

        const auto halfWidth = body.getWidth() / 2.0f;
        for (int side = 0; side < 2; ++side)
        {
            const auto half = body.withWidth (halfWidth).translated ((float) side * halfWidth, 0.0f).reduced (1.0f);
            const auto down = side == selected;
            drawRubberButton (g, half, down, false, 4.0f);

            g.setColour (down ? colours::ink : colours::inkDim);
            g.setFont (printFont (10.0f));
            g.drawText (labels[side], half.translated (0.0f, down ? 1.0f : 0.0f), juce::Justification::centred, false);

            // Small indicator bar on the chosen side, so the state reads without colour
            if (down)
            {
                g.setColour (colours::accent);
                g.fillRoundedRectangle (half.withTrimmedTop (half.getHeight() - 6.0f).reduced (half.getWidth() * 0.3f, 1.5f), 1.5f);
            }
        }
    }
}
