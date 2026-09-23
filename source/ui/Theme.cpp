#include "Theme.h"

namespace ui
{
    LookAndFeel::LookAndFeel()
    {
        setColour (juce::ResizableWindow::backgroundColourId, colours::background);
        setColour (juce::Label::textColourId, colours::text);
        setColour (juce::Slider::textBoxTextColourId, colours::textDim);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxHighlightColourId, colours::accent.withAlpha (0.3f));
        setColour (juce::ComboBox::backgroundColourId, colours::background);
        setColour (juce::ComboBox::outlineColourId, colours::outline);
        setColour (juce::ComboBox::textColourId, colours::text);
        setColour (juce::ComboBox::arrowColourId, colours::textDim);
        setColour (juce::PopupMenu::backgroundColourId, colours::panel);
        setColour (juce::PopupMenu::textColourId, colours::text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::accent.withAlpha (0.25f));
        setColour (juce::PopupMenu::highlightedTextColourId, colours::text);
        setColour (juce::TextButton::buttonColourId, colours::panel);
        setColour (juce::TextButton::textColourOffId, colours::textDim);
        setColour (juce::TextEditor::backgroundColourId, colours::background);
        setColour (juce::TextEditor::textColourId, colours::text);
        setColour (juce::TextEditor::highlightColourId, colours::accent.withAlpha (0.3f));
        setColour (juce::CaretComponent::caretColourId, colours::accent);
    }

    void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
        float rotaryStartAngle, float rotaryEndAngle, juce::Slider& slider)
    {
        const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (4.0f);
        const auto radius = std::min (bounds.getWidth(), bounds.getHeight()) / 2.0f;
        const auto centre = bounds.getCentre();
        const auto lineWidth = std::max (3.0f, radius * 0.14f);
        const auto arcRadius = radius - lineWidth / 2.0f;
        const auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
        const auto enabled = slider.isEnabled();

        // Track
        juce::Path track;
        track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour (colours::track);
        g.strokePath (track, juce::PathStrokeType (lineWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Value arc, from the origin value when one is set (e.g. 0 dB), otherwise from the minimum
        auto from = rotaryStartAngle;
        if (const auto* origin = slider.getProperties().getVarPointer (arcOriginProperty))
            from = rotaryStartAngle + (float) slider.valueToProportionOfLength ((double) *origin) * (rotaryEndAngle - rotaryStartAngle);
        if (std::abs (angle - from) > 0.01f)
        {
            juce::Path value;
            value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, std::min (from, angle), std::max (from, angle), true);
            g.setColour (enabled ? colours::accent : colours::textDim);
            g.strokePath (value, juce::PathStrokeType (lineWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Knob body and pointer
        const auto bodyRadius = arcRadius - lineWidth * 1.3f;
        g.setColour (colours::panel.brighter (0.08f));
        g.fillEllipse (juce::Rectangle<float> (bodyRadius * 2.0f, bodyRadius * 2.0f).withCentre (centre));

        const auto pointerStart = centre.getPointOnCircumference (bodyRadius * 0.35f, angle);
        const auto pointerEnd = centre.getPointOnCircumference (bodyRadius * 0.9f, angle);
        g.setColour (enabled ? colours::text : colours::textDim);
        g.drawLine ({ pointerStart, pointerEnd }, 2.0f);
    }

    void LookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box)
    {
        const auto bounds = juce::Rectangle<int> (0, 0, width, height).toFloat().reduced (0.5f);
        g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
        g.fillRoundedRectangle (bounds, 4.0f);
        g.setColour (box.hasKeyboardFocus (true) ? colours::accent.withAlpha (0.6f) : box.findColour (juce::ComboBox::outlineColourId));
        g.drawRoundedRectangle (bounds, 4.0f, 1.0f);

        // Chevron
        const auto arrow = juce::Rectangle<float> ((float) width - 20.0f, (float) height / 2.0f - 3.0f, 9.0f, 5.0f);
        juce::Path chevron;
        chevron.startNewSubPath (arrow.getX(), arrow.getY());
        chevron.lineTo (arrow.getCentreX(), arrow.getBottom());
        chevron.lineTo (arrow.getRight(), arrow.getY());
        g.setColour (box.findColour (juce::ComboBox::arrowColourId));
        g.strokePath (chevron, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    juce::Label* LookAndFeel::createSliderTextBox (juce::Slider& slider)
    {
        auto* label = LookAndFeel_V4::createSliderTextBox (slider);
        label->setFont (juce::FontOptions (12.0f));
        label->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
        label->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        label->setColour (juce::Label::textColourId, colours::textDim);
        label->setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        label->setColour (juce::TextEditor::focusedOutlineColourId, colours::accent);
        return label;
    }
}
