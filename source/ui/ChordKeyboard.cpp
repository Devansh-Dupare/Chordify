#include "ChordKeyboard.h"

namespace ui
{
    namespace
    {
        // Index of each semitone among the white keys of its octave (-1 for black keys)
        constexpr int whiteIndex[12] = { 0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6 };
        constexpr int whitesPerOctave = 7;
        constexpr float blackWidthRatio = 0.6f;
        constexpr float blackHeightRatio = 0.62f;
        constexpr int maxWindowStart = 12 * ((128 - ChordKeyboard::numKeys) / 12);

        const juce::Colour whiteKey { 0xffd9dce2 };
        const juce::Colour blackKey { 0xff262a33 };

        const juce::String defaultHint = "Click keys to pick up to 8 notes. Click an empty slot to save the chord there; right-click a slot to replace or clear it.";

        juce::RangedAudioParameter& slotParameter (juce::AudioProcessorValueTreeState& tree)
        {
            auto* param = tree.getParameter (params::id::chordSlot);
            jassert (param != nullptr);
            return *param;
        }
    }

    //==============================================================================
    ChordKeyboard::ChordKeyboard (juce::AudioProcessorValueTreeState& tree, ChordSlots& chordSlots)
        : slotParam (slotParameter (tree)),
          slots (chordSlots),
          slotAttachment (slotParam, [this] (float value) { activeSlot = juce::roundToInt (value); update(); })
    {
        slotAttachment.sendInitialUpdate();
        setRepaintsOnMouseActivity (false);
    }

    bool ChordKeyboard::isBlack (int note)
    {
        return whiteIndex[note % 12] < 0;
    }

    void ChordKeyboard::update()
    {
        const auto notes = slots.notesFor (activeSlot);

        // Only scroll when the chord's lowest note is off screen, and then put its octave first.
        // Scrolling on every edit would move keys out from under the mouse (a click can't hide
        // the lowest note, since only visible keys can be clicked).
        auto newWindowStart = windowStart;
        for (int note = 0; note < 128; ++note)
        {
            if (notes[(size_t) note])
            {
                if (note < windowStart || note >= windowStart + numKeys)
                    newWindowStart = std::min (12 * (note / 12), maxWindowStart);
                break;
            }
        }

        if (notes != shown || newWindowStart != windowStart)
        {
            const auto windowMoved = newWindowStart != windowStart;
            shown = notes;
            windowStart = newWindowStart;
            repaint();
            if (windowMoved && onWindowChanged != nullptr)
                onWindowChanged();
        }
    }

    void ChordKeyboard::refresh()
    {
        update();
    }

    bool ChordKeyboard::setNote (int note, bool on)
    {
        if (note < 0 || note > 127 || shown[(size_t) note] == on)
            return true;

        if (on && (int) shown.count() >= chordify::maxChordNotes)
        {
            if (onNoteLimit != nullptr)
                onNoteLimit();
            return false;
        }

        // Edits start from the chord shown and become the piano selection, which then plays
        auto notes = shown;
        notes[(size_t) note] = on;
        slots.setPiano (notes);

        if (activeSlot != params::pianoSlot)
            slotAttachment.setValueAsCompleteGesture ((float) params::pianoSlot);
        update();
        return true;
    }

    bool ChordKeyboard::toggleNote (int note)
    {
        return setNote (note, ! shown[(size_t) std::clamp (note, 0, 127)]);
    }

    void ChordKeyboard::clearNotes()
    {
        slots.setPiano ({});
        if (activeSlot != params::pianoSlot)
            slotAttachment.setValueAsCompleteGesture ((float) params::pianoSlot);
        update();
    }

    void ChordKeyboard::shiftOctave (int direction)
    {
        const auto shifted = std::clamp (windowStart + 12 * direction, 0, maxWindowStart);
        if (shifted == windowStart)
            return;

        windowStart = shifted;
        repaint();
        if (onWindowChanged != nullptr)
            onWindowChanged();
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
        if (const auto note = noteAt (event.position))
        {
            // The first key decides whether this drag adds or removes notes
            dragAdds = ! shown[(size_t) *note];
            lastDraggedNote = *note;
            setNote (*note, dragAdds);
        }
    }

    void ChordKeyboard::mouseDrag (const juce::MouseEvent& event)
    {
        if (const auto note = noteAt (event.position); note && *note != lastDraggedNote)
        {
            lastDraggedNote = *note;
            setNote (*note, dragAdds);
        }
    }

    void ChordKeyboard::paint (juce::Graphics& g)
    {
        for (auto black : { false, true })
        {
            for (int note = windowStart; note < windowStart + numKeys; ++note)
            {
                if (isBlack (note) != black)
                    continue;

                const auto key = keyBounds (note).reduced (black ? 0.0f : 1.0f, 0.0f);
                const auto on = shown[(size_t) note];
                g.setColour (on ? colours::accent : (black ? blackKey : whiteKey));
                g.fillRoundedRectangle (key.withTrimmedTop (-4.0f), 3.0f); // rounded bottom corners only

                if (black)
                {
                    g.setColour (colours::background);
                    g.drawRoundedRectangle (key.withTrimmedTop (-4.0f), 3.0f, 1.0f);
                }

                if (on || note % 12 == 0)
                {
                    // Label C keys, and every selected key so the chord can be read off the keyboard
                    const auto labelArea = key.withTop (key.getBottom() - 16.0f);
                    g.setColour (on ? colours::background : colours::textDim);
                    g.setFont (juce::FontOptions (black ? 8.5f : 10.0f, on ? juce::Font::bold : juce::Font::plain));
                    g.drawText (juce::MidiMessage::getMidiNoteName (note, true, ! black, 3), labelArea, juce::Justification::centred, false);
                }
            }
        }
    }

    //==============================================================================
    class ChordSlotButtons::SlotButton : public juce::Button
    {
    public:
        explicit SlotButton (int slotIndex) : juce::Button ("Slot " + juce::String (slotIndex + 1)), index (slotIndex) {}

        void setContents (const juce::String& noteText, bool isFilled, bool isActive)
        {
            if (noteText == notes && isFilled == filled && isActive == active)
                return;
            notes = noteText;
            filled = isFilled;
            active = isActive;
            setTooltip (filled ? "Slot " + juce::String (index + 1) + ": " + notes : "Empty: click to save the chord on the keyboard here");
            repaint();
        }

        std::function<void()> onRightClick;

        void mouseDown (const juce::MouseEvent& event) override
        {
            if (event.mods.isPopupMenu())
            {
                if (onRightClick != nullptr)
                    onRightClick();
                return;
            }
            juce::Button::mouseDown (event);
        }

        void paintButton (juce::Graphics& g, bool highlighted, bool down) override
        {
            auto bounds = getLocalBounds().toFloat().reduced (3.0f);
            const auto fill = active ? colours::accent : (filled ? colours::panel.brighter (0.1f) : juce::Colours::transparentBlack);
            g.setColour (fill.brighter (down ? 0.15f : (highlighted ? 0.07f : 0.0f)));
            g.fillRoundedRectangle (bounds, 6.0f);

            if (! active)
            {
                g.setColour (filled ? colours::outline : colours::textDim.withAlpha (highlighted ? 0.7f : 0.4f));
                g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);
            }

            bounds.reduce (8.0f, 4.0f);
            const auto textColour = active ? colours::background : colours::text;
            g.setColour (textColour);
            g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
            g.drawText (juce::String (index + 1), bounds.removeFromTop (bounds.getHeight() * 0.5f), juce::Justification::bottomLeft, false);

            g.setColour (filled ? textColour.withAlpha (0.85f) : colours::textDim);
            g.setFont (juce::FontOptions (10.5f));
            g.drawFittedText (filled ? notes : juce::String ("Empty"), bounds.toNearestInt(), juce::Justification::topLeft, 1, 0.8f);
        }

    private:
        int index;
        juce::String notes;
        bool filled = false, active = false;
    };

    ChordSlotButtons::ChordSlotButtons (juce::AudioProcessorValueTreeState& tree, ChordSlots& chordSlots)
        : slotParam (slotParameter (tree)),
          slots (chordSlots),
          attachment (slotParam, [this] (float value) { activeSlot = juce::roundToInt (value); refresh(); })
    {
        for (int i = 0; i < ChordSlots::numSlots; ++i)
        {
            auto* button = buttons.add (std::make_unique<SlotButton> (i));
            button->onClick = [this, i] { clickSlot (i); };
            button->onRightClick = [this, i] { showMenu (i); };
            addAndMakeVisible (button);
        }
        attachment.sendInitialUpdate();
    }

    ChordSlotButtons::~ChordSlotButtons() = default;

    ChordSlots::Notes ChordSlotButtons::shownNotes() const
    {
        return slots.notesFor (activeSlot);
    }

    void ChordSlotButtons::playSlot (int index)
    {
        attachment.setValueAsCompleteGesture ((float) (index + 1));
        refresh();
    }

    void ChordSlotButtons::clickSlot (int index)
    {
        if (slots.getSlot (index).any())
        {
            playSlot (index);
            return;
        }

        if (shownNotes().none())
        {
            if (onNothingToSave != nullptr)
                onNothingToSave();
            return;
        }

        saveToSlot (index);
        playSlot (index);
    }

    void ChordSlotButtons::saveToSlot (int index)
    {
        slots.setSlot (index, shownNotes());
        refresh();
    }

    void ChordSlotButtons::clearSlot (int index)
    {
        slots.setSlot (index, {});
        refresh();
    }

    void ChordSlotButtons::showMenu (int index)
    {
        const auto filled = slots.getSlot (index).any();
        const auto haveNotes = shownNotes().any();

        juce::PopupMenu menu;
        menu.addItem (filled ? "Replace with the chord on the keyboard" : "Save the chord on the keyboard here", haveNotes, false,
            [this, index] { saveToSlot (index); });
        menu.addItem ("Clear slot", filled, false, [this, index] { clearSlot (index); });
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (buttons[index]));
    }

    void ChordSlotButtons::refresh()
    {
        for (int i = 0; i < buttons.size(); ++i)
        {
            const auto notes = slots.getSlot (i);
            buttons[i]->setContents (ChordSlots::noteNames (notes), notes.any(), activeSlot == i + 1);
        }
    }

    void ChordSlotButtons::resized()
    {
        const auto width = (float) getWidth() / (float) buttons.size();
        for (int i = 0; i < buttons.size(); ++i)
            buttons[i]->setBounds (juce::Rectangle<float> ((float) i * width, 0.0f, width, (float) getHeight()).toNearestInt());
    }

    //==============================================================================
    ChordPanel::ChordPanel (juce::AudioProcessorValueTreeState& tree, ChordSlots& chordSlots)
        : keyboard (tree, chordSlots), slotButtons (tree, chordSlots)
    {
        octaveCaption.setText ("Octave", juce::dontSendNotification);
        octaveCaption.setColour (juce::Label::textColourId, colours::textDim);
        octaveCaption.setFont (juce::FontOptions (12.5f));
        octaveRange.setJustificationType (juce::Justification::centred);
        octaveRange.setFont (juce::FontOptions (12.0f));
        hint.setColour (juce::Label::textColourId, colours::textDim);
        hint.setFont (juce::FontOptions (12.0f));
        hint.setText (defaultHint, juce::dontSendNotification);

        octaveDown.setTooltip ("Scroll the keyboard down an octave");
        octaveUp.setTooltip ("Scroll the keyboard up an octave");
        clear.setTooltip ("Remove every note from the keyboard");
        octaveDown.onClick = [this] { keyboard.shiftOctave (-1); };
        octaveUp.onClick = [this] { keyboard.shiftOctave (1); };
        clear.onClick = [this] { keyboard.clearNotes(); };

        keyboard.onWindowChanged = [this] { updateRange(); };
        keyboard.onNoteLimit = [this] { flashHint ("A chord can have up to 8 notes. Remove one to add another."); };
        slotButtons.onNothingToSave = [this] { flashHint ("Pick some notes on the keyboard first, then click an empty slot to save them."); };
        updateRange();

        for (auto* child : std::initializer_list<juce::Component*> { &keyboard, &slotButtons, &octaveCaption, &octaveRange, &hint, &octaveDown, &octaveUp, &clear })
            addAndMakeVisible (child);
    }

    void ChordPanel::refresh()
    {
        keyboard.refresh();
        slotButtons.refresh();
    }

    void ChordPanel::flashHint (const juce::String& message)
    {
        hint.setText (message, juce::dontSendNotification);
        hint.setColour (juce::Label::textColourId, colours::warm);
        startTimer (3000);
    }

    void ChordPanel::timerCallback()
    {
        stopTimer();
        hint.setText (defaultHint, juce::dontSendNotification);
        hint.setColour (juce::Label::textColourId, colours::textDim);
    }

    void ChordPanel::updateRange()
    {
        const auto low = keyboard.getLowestNote();
        octaveRange.setText (juce::MidiMessage::getMidiNoteName (low, true, true, 3) + juce::String::fromUTF8 (" \xe2\x80\x93 ")
                                 + juce::MidiMessage::getMidiNoteName (low + ChordKeyboard::numKeys - 1, true, true, 3),
            juce::dontSendNotification);
    }

    void ChordPanel::resized()
    {
        constexpr int controlsWidth = 124;
        constexpr int gap = 12;

        auto area = getLocalBounds();
        auto slotsRow = area.removeFromBottom (48);
        slotButtons.setBounds (slotsRow);
        area.removeFromBottom (4);
        hint.setBounds (area.removeFromBottom (20));
        area.removeFromBottom (6);

        auto controls = area.removeFromLeft (controlsWidth);
        area.removeFromLeft (gap);
        keyboard.setBounds (area);

        octaveCaption.setBounds (controls.removeFromTop (18));
        auto row = controls.removeFromTop (28);
        octaveDown.setBounds (row.removeFromLeft (28));
        octaveUp.setBounds (row.removeFromRight (28));
        octaveRange.setBounds (row);
        controls.removeFromTop (10);
        clear.setBounds (controls.removeFromTop (28));
    }
}
