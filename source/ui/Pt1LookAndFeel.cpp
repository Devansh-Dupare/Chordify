#include "Pt1LookAndFeel.h"

namespace pt1
{
    namespace
    {
        constexpr int knobTicks = 11;
        constexpr size_t maxCachedKnobs = 32; // a handful of sizes x scales x states

        juce::Path ellipse (juce::Point<float> centre, float radius)
        {
            juce::Path p;
            p.addEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre));
            return p;
        }
    }

    void drawRubberButton (juce::Graphics& g, juce::Rectangle<float> bounds, bool down, bool highlighted, float cornerSize)
    {
        juce::Path body;
        body.addRoundedRectangle (bounds, cornerSize);

        if (! down)
            juce::DropShadow (light::shadow.withAlpha (light::shadowAlpha), light::shadowRadius / 2, light::shadowOffset / 2).drawForPath (g, body);

        const auto top = colours::caseShade.brighter (down ? -0.02f : (highlighted ? 0.12f : 0.08f));
        const auto bottom = colours::caseShade.darker (down ? -0.02f : 0.12f);
        g.setGradientFill (juce::ColourGradient::vertical (down ? bottom : top, bounds.getY(), down ? top : bottom, bounds.getBottom()));
        g.fillPath (body);

        // Bevel: light on the upper edge when raised, shadow along it when pushed in
        g.setColour (down ? juce::Colours::black.withAlpha (0.12f) : juce::Colours::white.withAlpha (0.6f));
        g.drawRoundedRectangle (bounds.reduced (1.0f).withTrimmedBottom (bounds.getHeight() * 0.5f), cornerSize - 1.0f, 1.0f);
        g.setColour (colours::caseEdge);
        g.drawRoundedRectangle (bounds.reduced (0.5f), cornerSize, 1.0f);
    }

    LookAndFeel::LookAndFeel()
    {
        setColour (juce::ResizableWindow::backgroundColourId, colours::caseTop);
        setColour (juce::Label::textColourId, colours::ink);
        setColour (juce::Slider::textBoxTextColourId, colours::inkDim);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxHighlightColourId, colours::accent.withAlpha (0.3f));
        setColour (juce::TextButton::textColourOffId, colours::ink);
        setColour (juce::TextButton::textColourOnId, colours::ink);
        setColour (juce::ComboBox::textColourId, colours::ink);
        setColour (juce::ComboBox::arrowColourId, colours::inkDim);
        setColour (juce::PopupMenu::backgroundColourId, colours::caseTop);
        setColour (juce::PopupMenu::textColourId, colours::ink);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::accent.withAlpha (0.2f));
        setColour (juce::PopupMenu::highlightedTextColourId, colours::ink);
        setColour (juce::TextEditor::backgroundColourId, colours::caseTop);
        setColour (juce::TextEditor::textColourId, colours::ink);
        setColour (juce::TextEditor::highlightColourId, colours::accent.withAlpha (0.3f));
        setColour (juce::CaretComponent::caretColourId, colours::accent);
        setColour (juce::TooltipWindow::backgroundColourId, colours::caseTop);
        setColour (juce::TooltipWindow::textColourId, colours::ink);
    }

    //==============================================================================
    const juce::Image& LookAndFeel::knobLayers (float diameter, float scale, float startAngle, float endAngle, bool pressed)
    {
        const auto key = std::make_tuple (juce::roundToInt (diameter * 4.0f), juce::roundToInt (scale * 100.0f),
            juce::roundToInt (startAngle * 1000.0f), juce::roundToInt (endAngle * 1000.0f), pressed);

        if (const auto found = knobCache.find (key); found != knobCache.end())
            return found->second;

        if (knobCache.size() >= maxCachedKnobs)
            knobCache.clear();

        const auto bounds = juce::Rectangle<float> (diameter, diameter);
        const auto image = renderCached (bounds, scale, [&] (juce::Graphics& g) {
            const auto centre = bounds.getCentre();
            const auto tickRadius = diameter * 0.47f;
            const auto skirtRadius = diameter * 0.39f;
            const auto capRadius = diameter * 0.30f;

            // Printed scale: dots around the knob, larger at both ends
            g.setColour (colours::ink.withAlpha (0.55f));
            for (int i = 0; i < knobTicks; ++i)
            {
                const auto angle = startAngle + (endAngle - startAngle) * (float) i / (float) (knobTicks - 1);
                const auto size = (i == 0 || i == knobTicks - 1) ? diameter * 0.035f : diameter * 0.022f;
                g.fillEllipse (juce::Rectangle<float> (size * 2.0f, size * 2.0f).withCentre (centre.getPointOnCircumference (tickRadius, angle)));
            }

            // 1. Soft ambient shadow on the case, offset away from the light
            juce::DropShadow (light::shadow.withAlpha (pressed ? 0.25f : 0.38f), juce::roundToInt (skirtRadius * (pressed ? 0.25f : 0.4f)),
                { 0, juce::roundToInt (skirtRadius * (pressed ? 0.08f : 0.16f)) })
                .drawForPath (g, ellipse (centre, skirtRadius));

            // 2. Skirt: a ring a shade darker than the case
            g.setGradientFill (juce::ColourGradient::vertical (colours::caseShade.brighter (0.05f), centre.y - skirtRadius,
                colours::caseShade.darker (0.18f), centre.y + skirtRadius));
            g.fillPath (ellipse (centre, skirtRadius));
            g.setColour (colours::caseEdge.darker (0.1f));
            g.strokePath (ellipse (centre, skirtRadius), juce::PathStrokeType (1.0f));

            // Shadow of the cap on the skirt
            juce::DropShadow (light::shadow.withAlpha (0.35f), juce::roundToInt (capRadius * 0.25f), { 0, juce::roundToInt (capRadius * 0.1f) })
                .drawForPath (g, ellipse (centre, capRadius));

            // 3. Cap: radial gradient, brightest where it faces the light
            const auto highlightPoint = centre + light::towardsLight * capRadius * 0.55f;
            juce::ColourGradient capFill (colours::capLight, highlightPoint, colours::capDark, centre - light::towardsLight * capRadius, true);
            capFill.addColour (0.55, colours::capLight.interpolatedWith (colours::capDark, 0.55f));
            g.setGradientFill (capFill);
            g.fillPath (ellipse (centre, capRadius));

            if (pressed)
            {
                g.setColour (juce::Colours::black.withAlpha (0.12f));
                g.fillPath (ellipse (centre, capRadius));
            }

            // 5. Rim highlight along the edge that faces the light
            juce::Path rim;
            rim.addCentredArc (centre.x, centre.y, capRadius - 0.75f, capRadius - 0.75f, 0.0f,
                light::highlightAngle - 1.1f, light::highlightAngle + 1.1f, true);
            g.setColour (juce::Colours::white.withAlpha (0.35f));
            g.strokePath (rim, juce::PathStrokeType (1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (juce::Colours::black.withAlpha (0.5f));
            g.strokePath (ellipse (centre, capRadius), juce::PathStrokeType (0.8f));
        });

        return knobCache.emplace (key, image).first->second;
    }

    void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
        float rotaryStartAngle, float rotaryEndAngle, juce::Slider& slider)
    {
        // Leave room around the knob so the printed scale doesn't crowd its neighbours
        const auto diameter = (float) std::min (width, height) * 0.86f;
        const auto bounds = juce::Rectangle<float> (diameter, diameter).withCentre (juce::Rectangle<int> (x, y, width, height).toFloat().getCentre());
        const auto pressed = slider.isMouseButtonDown();

        // Static layers come from the cache; only the indicator is drawn fresh
        g.drawImage (knobLayers (diameter, physicalScale (g), rotaryStartAngle, rotaryEndAngle, pressed), bounds);

        // 4. Indicator line, crisp on top of the soft cap
        const auto centre = bounds.getCentre();
        const auto capRadius = diameter * 0.30f;
        const auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
        g.setColour (slider.isEnabled() ? colours::accent : colours::inkDim);
        g.drawLine ({ centre.getPointOnCircumference (capRadius * 0.3f, angle), centre.getPointOnCircumference (capRadius * 0.88f, angle) },
            std::max (2.0f, capRadius * 0.13f));
    }

    //==============================================================================
    void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&, bool highlighted, bool down)
    {
        drawRubberButton (g, button.getLocalBounds().toFloat().reduced (2.0f, 3.0f), down, highlighted);
    }

    void LookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button, bool, bool down)
    {
        g.setColour (colours::ink.withMultipliedAlpha (button.isEnabled() ? 1.0f : 0.5f));
        g.setFont (printFont (std::min (13.0f, (float) button.getHeight() * 0.42f)));
        g.drawText (button.getButtonText().toUpperCase(), button.getLocalBounds().translated (0, down ? 1 : 0), juce::Justification::centred, false);
    }

    //==============================================================================
    void LookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown, int, int, int, int, juce::ComboBox& box)
    {
        drawRubberButton (g, juce::Rectangle<int> (width, height).toFloat().reduced (1.0f, 2.0f), isButtonDown, box.isMouseOver (true), 5.0f);

        const auto arrow = juce::Rectangle<float> ((float) width - 20.0f, (float) height / 2.0f - 2.5f, 9.0f, 5.0f);
        juce::Path chevron;
        chevron.startNewSubPath (arrow.getX(), arrow.getY());
        chevron.lineTo (arrow.getCentreX(), arrow.getBottom());
        chevron.lineTo (arrow.getRight(), arrow.getY());
        g.setColour (colours::inkDim);
        g.strokePath (chevron, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    juce::Font LookAndFeel::getComboBoxFont (juce::ComboBox&)
    {
        return juce::Font (textFont (14.0f, true));
    }

    void LookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
    {
        label.setBounds (8, 1, box.getWidth() - 30, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
    }

    void LookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
    {
        g.fillAll (colours::caseTop);
        g.setColour (colours::caseEdge);
        g.drawRect (0, 0, width, height);
    }

    juce::Font LookAndFeel::getPopupMenuFont()
    {
        return juce::Font (textFont (14.0f));
    }

    juce::Label* LookAndFeel::createSliderTextBox (juce::Slider& slider)
    {
        auto* label = LookAndFeel_V4::createSliderTextBox (slider);
        label->setFont (textFont (11.5f));
        label->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
        label->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        label->setColour (juce::Label::textColourId, colours::inkDim);
        label->setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        label->setColour (juce::TextEditor::focusedOutlineColourId, colours::accent);
        return label;
    }
}
