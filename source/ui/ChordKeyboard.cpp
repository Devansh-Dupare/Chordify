#include "ChordKeyboard.h"
#include "../dsp/ChordMapper.h"
#include "../params/Parameters.h"

namespace ui
{
    namespace
    {
        // Index of each semitone among the white keys of its octave (-1 for black keys)
        constexpr int whiteIndex[12] = { 0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6 };
        constexpr int whitesPerOctave = 7;
        constexpr float blackWidthRatio = 0.6f;
        constexpr float blackHeightRatio = 0.62f;

        const juce::Colour whiteKey { 0xffd9dce2 };
        const juce::Colour blackKey { 0xff262a33 };

        juce::RangedAudioParameter& parameter (juce::AudioProcessorValueTreeState& tree, const char* paramId)
        {
            auto* param = tree.getParameter (paramId);
            jassert (param != nullptr);
            return *param;
        }
    }

    ChordKeyboard::ChordKeyboard (juce::AudioProcessorValueTreeState& tree, std::function<SoundingChord()> sounding)
        : rootParam (parameter (tree, params::id::root)),
          chordTypeParam (parameter (tree, params::id::chordType)),
          sourceParam (parameter (tree, params::id::chordSource)),
          rootAttachment (rootParam, [this] (float value) { root = juce::roundToInt (value); update(); }),
          chordTypeAttachment (chordTypeParam, [this] (float value) { chordType = juce::roundToInt (value); update(); }),
          sourceAttachment (sourceParam, [this] (float value) { source = juce::roundToInt (value); update(); }),
          soundingChord (std::move (sounding))
    {
        rootAttachment.sendInitialUpdate();
        chordTypeAttachment.sendInitialUpdate();
        sourceAttachment.sendInitialUpdate();
        setRepaintsOnMouseActivity (false);
    }

    bool ChordKeyboard::isBlack (int note)
    {
        return whiteIndex[note % 12] < 0;
    }

    void ChordKeyboard::update()
    {
        std::bitset<128> notes;
        int newRoot = -1;

        // Only scroll when a note would be off screen, and then put its octave first. Otherwise
        // clicking a key in the upper octave would scroll the keyboard out from under the mouse.
        auto newWindowStart = windowStart;
        const auto keepVisible = [&newWindowStart] (int note) {
            if (note < newWindowStart || note >= newWindowStart + numKeys)
                newWindowStart = 12 * (note / 12);
        };

        if ((params::ChordSource) source == params::ChordSource::internal)
        {
            // From the parameters, so the keyboard is right even when no audio is being processed
            for (auto interval : chordify::chordIntervals (chordType))
                if (root + interval < 128)
                    notes[(size_t) (root + interval)] = true;
            newRoot = root;
            keepVisible (root);
        }
        else if (soundingChord != nullptr)
        {
            const auto sounding = soundingChord();
            notes = sounding.notes;
            newRoot = sounding.root;

            // Keep the sounding chord's lowest note on screen
            for (int note = 0; note < 128; ++note)
            {
                if (notes[(size_t) note])
                {
                    keepVisible (note);
                    break;
                }
            }
        }

        newWindowStart = std::clamp (newWindowStart, 0, 12 * ((127 - numKeys) / 12)); // always starts on a C

        if (notes != highlighted || newRoot != highlightedRoot || newWindowStart != windowStart)
        {
            highlighted = notes;
            highlightedRoot = newRoot;
            const auto windowMoved = newWindowStart != windowStart;
            windowStart = newWindowStart;
            repaint();
            if (windowMoved && onWindowChanged != nullptr)
                onWindowChanged();
        }
    }

    void ChordKeyboard::refresh()
    {
        if ((params::ChordSource) source != params::ChordSource::internal)
            update();
    }

    void ChordKeyboard::selectRoot (int note)
    {
        const auto range = rootParam.getNormalisableRange();
        if (note < (int) range.start || note > (int) range.end)
            return;

        if ((params::ChordSource) source != params::ChordSource::internal)
            sourceAttachment.setValueAsCompleteGesture ((float) params::ChordSource::internal);

        rootAttachment.setValueAsCompleteGesture ((float) note);
    }

    void ChordKeyboard::shiftOctave (int direction)
    {
        const auto range = rootParam.getNormalisableRange();
        const auto shifted = root + 12 * direction;
        if (shifted < (int) range.start || shifted > (int) range.end)
            return;

        // Scroll with the root, so the chord keeps its place on screen
        windowStart = std::clamp (windowStart + 12 * direction, 0, 12 * ((127 - numKeys) / 12));
        if (onWindowChanged != nullptr)
            onWindowChanged();

        rootAttachment.setValueAsCompleteGesture ((float) shifted);
        repaint();
    }

    //==============================================================================
    juce::Rectangle<float> ChordKeyboard::keyBounds (int note) const
    {
        const auto bounds = getLocalBounds().toFloat();
        const auto whiteWidth = bounds.getWidth() / (float) (numOctaves * whitesPerOctave);
        const auto offset = note - windowStart;
        const auto octave = offset / 12;
        const auto semitone = offset % 12;

        if (! isBlack (note))
            return { bounds.getX() + (float) (octave * whitesPerOctave + whiteIndex[semitone]) * whiteWidth, bounds.getY(), whiteWidth, bounds.getHeight() };

        // A black key sits across the boundary after the white key below it
        const auto whiteBelow = octave * whitesPerOctave + whiteIndex[semitone - 1];
        const auto blackWidth = whiteWidth * blackWidthRatio;
        return { bounds.getX() + (float) (whiteBelow + 1) * whiteWidth - blackWidth / 2.0f, bounds.getY(), blackWidth, bounds.getHeight() * blackHeightRatio };
    }

    std::optional<int> ChordKeyboard::noteAt (juce::Point<float> position) const
    {
        // Black keys sit on top, so test them first
        for (auto black : { true, false })
            for (int note = windowStart; note < windowStart + numKeys; ++note)
                if (isBlack (note) == black && keyBounds (note).contains (position))
                    return note;
        return std::nullopt;
    }

    void ChordKeyboard::mouseDown (const juce::MouseEvent& event)
    {
        lastDraggedNote = -1;
        mouseDrag (event);
    }

    void ChordKeyboard::mouseDrag (const juce::MouseEvent& event)
    {
        if (const auto note = noteAt (event.position); note && *note != lastDraggedNote)
        {
            lastDraggedNote = *note;
            selectRoot (*note);
        }
    }

    void ChordKeyboard::paint (juce::Graphics& g)
    {
        const auto range = rootParam.getNormalisableRange();
        const auto internal = (params::ChordSource) source == params::ChordSource::internal;

        const auto keyColour = [&] (int note, juce::Colour base) {
            if (note == highlightedRoot)
                return colours::accent;
            if (highlighted[(size_t) note])
                return base.interpolatedWith (colours::accent, 0.55f);
            if (note < (int) range.start || note > (int) range.end)
                return base.interpolatedWith (colours::panel, 0.5f); // outside Root's range: not clickable
            return base;
        };

        for (auto black : { false, true })
        {
            for (int note = windowStart; note < windowStart + numKeys; ++note)
            {
                if (isBlack (note) != black)
                    continue;

                const auto key = keyBounds (note).reduced (black ? 0.0f : 1.0f, 0.0f);
                g.setColour (keyColour (note, black ? blackKey : whiteKey).withMultipliedAlpha (internal ? 1.0f : 0.85f));
                g.fillRoundedRectangle (key.withTrimmedTop (-4.0f), 3.0f); // rounded bottom corners only

                if (black)
                {
                    g.setColour (colours::background);
                    g.drawRoundedRectangle (key.withTrimmedTop (-4.0f), 3.0f, 1.0f);
                }
                else if (note % 12 == 0)
                {
                    g.setColour (note == highlightedRoot ? colours::background : colours::textDim);
                    g.setFont (juce::FontOptions (10.0f));
                    g.drawText (juce::MidiMessage::getMidiNoteName (note, true, true, 3), key.withTop (key.getBottom() - 16.0f),
                        juce::Justification::centred, false);
                }
            }
        }
    }

    //==============================================================================
    ChordTypeButtons::ChordTypeButtons (juce::AudioProcessorValueTreeState& tree)
        : param (parameter (tree, params::id::chordType)),
          attachment (param, [this] (float value) {
              const auto selected = juce::roundToInt (value);
              for (int i = 0; i < buttons.size(); ++i)
                  buttons[i]->setToggleState (i == selected, juce::dontSendNotification);
          })
    {
        for (int i = 0; i < params::chordTypeShortNames.size(); ++i)
        {
            auto* button = buttons.add (std::make_unique<juce::TextButton> (params::chordTypeShortNames[i]));
            button->setTooltip (params::chordTypeNames[i]);
            button->setColour (juce::TextButton::buttonOnColourId, colours::accent);
            button->setColour (juce::TextButton::textColourOnId, colours::background);
            button->setConnectedEdges (0);
            button->onClick = [this, i] { attachment.setValueAsCompleteGesture ((float) i); };
            addAndMakeVisible (button);
        }
        attachment.sendInitialUpdate();
    }

    void ChordTypeButtons::resized()
    {
        constexpr int cellHeight = 34;
        const auto rows = (buttons.size() + columns - 1) / columns;
        const auto cellWidth = getWidth() / columns;
        const auto top = (getHeight() - rows * cellHeight) / 2;
        for (int i = 0; i < buttons.size(); ++i)
            buttons[i]->setBounds (juce::Rectangle<int> ((i % columns) * cellWidth, top + (i / columns) * cellHeight, cellWidth, cellHeight).reduced (3));
    }

    //==============================================================================
    OctaveButtons::OctaveButtons (ChordKeyboard& keyboardToControl) : keyboard (keyboardToControl)
    {
        down.onClick = [this] { keyboard.shiftOctave (-1); };
        up.onClick = [this] { keyboard.shiftOctave (1); };
        down.setTooltip ("Root down an octave");
        up.setTooltip ("Root up an octave");

        range.setJustificationType (juce::Justification::centred);
        range.setFont (juce::FontOptions (12.5f));
        range.setColour (juce::Label::textColourId, colours::text);

        caption.setText ("Keyboard", juce::dontSendNotification);
        caption.setFont (juce::FontOptions (12.5f));
        caption.setColour (juce::Label::textColourId, colours::textDim);

        keyboard.onWindowChanged = [this] { updateRange(); };
        updateRange();

        for (auto* child : std::initializer_list<juce::Component*> { &caption, &down, &range, &up })
            addAndMakeVisible (child);
    }

    void OctaveButtons::updateRange()
    {
        const auto low = keyboard.getLowestNote();
        range.setText (juce::MidiMessage::getMidiNoteName (low, true, true, 3) + juce::String::fromUTF8 (" \xe2\x80\x93 ")
                           + juce::MidiMessage::getMidiNoteName (low + ChordKeyboard::numKeys - 1, true, true, 3),
            juce::dontSendNotification);
    }

    void OctaveButtons::resized()
    {
        auto area = getLocalBounds();
        caption.setBounds (area.removeFromTop (16));
        auto row = area.removeFromTop (26);
        down.setBounds (row.removeFromLeft (30));
        up.setBounds (row.removeFromRight (30));
        range.setBounds (row);
    }
}
