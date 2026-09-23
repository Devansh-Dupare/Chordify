#include "Widgets.h"

namespace ui
{
    namespace
    {
        constexpr int labelHeight = 16;
        constexpr int valueHeight = 18;

        void styleLabel (juce::Label& label, const juce::String& text, juce::Justification justification)
        {
            label.setText (text, juce::dontSendNotification);
            label.setJustificationType (justification);
            label.setFont (juce::FontOptions (12.5f));
            label.setColour (juce::Label::textColourId, colours::textDim);
        }
    }

    //==============================================================================
    Knob::Knob (juce::AudioProcessorValueTreeState& tree, const juce::String& paramId, std::optional<float> arcOrigin)
        : attachment (tree, paramId, slider)
    {
        auto* param = tree.getParameter (paramId);
        styleLabel (label, param->getName (32), juce::Justification::centred);

        // Show the unit in the value box. Hosts show the parameter's label separately, so it isn't
        // part of the parameter's own text. Typed values still parse ("1.5 s" and "1.5" both work).
        if (const auto unit = param->getLabel(); unit.isNotEmpty())
            slider.textFromValueFunction = [param, unit] (double value) {
                return param->getText (param->convertTo0to1 ((float) value), 32) + " " + unit;
            };
        slider.updateText();

        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, preferredWidth, valueHeight);
        if (arcOrigin)
            slider.getProperties().set (arcOriginProperty, *arcOrigin);
        slider.setPopupDisplayEnabled (false, false, nullptr);
        addAndMakeVisible (label);
        addAndMakeVisible (slider);
    }

    void Knob::resized()
    {
        auto area = getLocalBounds();
        label.setBounds (area.removeFromTop (labelHeight));
        slider.setBounds (area);
    }

    //==============================================================================
    Choice::Choice (juce::AudioProcessorValueTreeState& tree, const juce::String& paramId)
    {
        // Fill the items before attaching, so the attachment can select the current one
        if (auto* param = dynamic_cast<juce::AudioParameterChoice*> (tree.getParameter (paramId)))
            box.addItemList (param->choices, 1);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (tree, paramId, box);

        styleLabel (label, tree.getParameter (paramId)->getName (32), juce::Justification::centredLeft);
        addAndMakeVisible (label);
        addAndMakeVisible (box);
    }

    void Choice::resized()
    {
        auto area = getLocalBounds();
        label.setBounds (area.removeFromTop (labelHeight));
        box.setBounds (area.removeFromTop (26));
    }

    //==============================================================================
    Section::Section (juce::String titleText) : title (std::move (titleText)) {}

    void Section::addItem (juce::Component& item, int width)
    {
        items.emplace_back (&item, width);
        addAndMakeVisible (item);
    }

    int Section::getMinimumWidth() const
    {
        auto width = padding;
        for (const auto& [item, itemWidth] : items)
            width += itemWidth + padding;
        return width;
    }

    void Section::paint (juce::Graphics& g)
    {
        const auto bounds = getLocalBounds().toFloat();
        g.setColour (colours::panel);
        g.fillRoundedRectangle (bounds, 8.0f);
        g.setColour (colours::outline);
        g.drawRoundedRectangle (bounds.reduced (0.5f), 8.0f, 1.0f);

        g.setColour (colours::accent);
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText (title.toUpperCase(), getLocalBounds().removeFromTop (titleHeight).withTrimmedLeft (padding + 2).withTrimmedTop (6),
            juce::Justification::topLeft, false);
    }

    void Section::resized()
    {
        auto area = getLocalBounds().withTrimmedTop (titleHeight).reduced (0, padding / 2);

        int used = 0;
        for (const auto& [item, width] : items)
            used += width;
        const auto gap = std::max (padding, (area.getWidth() - used) / ((int) items.size() + 1));

        auto x = area.getX() + gap;
        for (const auto& [item, width] : items)
        {
            item->setBounds (x, area.getY(), width, area.getHeight());
            x += width + gap;
        }
    }

    //==============================================================================
    void Stack::add (juce::Component& item)
    {
        items.push_back (&item);
        addAndMakeVisible (item);
    }

    void Stack::resized()
    {
        auto area = getLocalBounds();
        const auto height = area.getHeight() / std::max (1, (int) items.size());
        for (auto* item : items)
            item->setBounds (area.removeFromTop (height));
    }

    //==============================================================================
    void ChordDisplay::setChord (const juce::String& name, const juce::String& detail, const juce::String& footnote)
    {
        if (name == chordName && detail == chordDetail && footnote == footnoteText)
            return;
        chordName = name;
        chordDetail = detail;
        footnoteText = footnote;
        repaint();
    }

    void ChordDisplay::paint (juce::Graphics& g)
    {
        auto bounds = getLocalBounds();
        g.setColour (colours::panel);
        g.fillRoundedRectangle (bounds.toFloat(), 8.0f);
        g.setColour (colours::outline);
        g.drawRoundedRectangle (bounds.toFloat().reduced (0.5f), 8.0f, 1.0f);

        bounds.reduce (16, 8);
        auto top = bounds.removeFromTop (bounds.getHeight() * 3 / 5);

        g.setColour (colours::text);
        g.setFont (juce::FontOptions (26.0f, juce::Font::bold));
        const auto nameWidth = std::min (top.getWidth() / 2, juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), chordName) + 20);
        g.drawText (chordName, top.removeFromLeft (nameWidth), juce::Justification::bottomLeft, true);

        g.setColour (colours::accent);
        g.setFont (juce::FontOptions (15.0f));
        g.drawText (chordDetail, top, juce::Justification::bottomLeft, true);

        g.setColour (colours::textDim);
        g.setFont (juce::FontOptions (12.0f));
        g.drawText (footnoteText, bounds, juce::Justification::centredLeft, true);
    }

    //==============================================================================
    void LevelMeter::update (std::array<float, 2> peaks)
    {
        constexpr float fallDbPerFrame = 1.2f; // ~36 dB/s at 30 fps
        constexpr int holdFramesMax = 45;

        for (size_t ch = 0; ch < 2; ++ch)
        {
            const auto db = juce::jlimit (minDb, maxDb, juce::Decibels::gainToDecibels (peaks[ch], minDb));
            levelDb[ch] = std::max (db, levelDb[ch] - fallDbPerFrame);

            if (db >= holdDb[ch] || holdFrames[ch]-- <= 0)
            {
                holdDb[ch] = db;
                holdFrames[ch] = holdFramesMax;
            }
        }
        repaint();
    }

    void LevelMeter::paint (juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();
        const auto toX = [&] (float db) { return bounds.getX() + bounds.getWidth() * (db - minDb) / (maxDb - minDb); };
        const auto barHeight = (bounds.getHeight() - 16.0f - 4.0f) / 2.0f;

        for (size_t ch = 0; ch < 2; ++ch)
        {
            const auto bar = juce::Rectangle<float> (bounds.getX(), bounds.getY() + (float) ch * (barHeight + 4.0f), bounds.getWidth(), barHeight);
            g.setColour (colours::track);
            g.fillRoundedRectangle (bar, 2.0f);

            const auto level = levelDb[ch];
            const auto colour = level > 0.0f ? colours::hot : (level > -6.0f ? colours::warm : colours::accent);
            g.setColour (colour);
            g.fillRoundedRectangle (bar.withRight (toX (level)), 2.0f);

            if (holdDb[ch] > minDb)
            {
                g.setColour (holdDb[ch] > 0.0f ? colours::hot : colours::text);
                g.fillRect (juce::Rectangle<float> (toX (holdDb[ch]) - 1.0f, bar.getY(), 2.0f, bar.getHeight()));
            }
        }

        // 0 dB mark and scale
        g.setColour (colours::textDim);
        g.drawVerticalLine ((int) toX (0.0f), bounds.getY(), bounds.getY() + 2.0f * barHeight + 4.0f);
        g.setFont (juce::FontOptions (10.0f));
        for (auto db : { -48.0f, -24.0f, -12.0f, -6.0f, 0.0f })
            g.drawText (juce::String ((int) db), juce::Rectangle<float> (toX (db) - 15.0f, bounds.getBottom() - 14.0f, 30.0f, 14.0f),
                juce::Justification::centred, false);
    }
}
