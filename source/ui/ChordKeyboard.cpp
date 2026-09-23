#include "ChordKeyboard.h"
#include "Pt1LookAndFeel.h"

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

        // Key depth: how much of each key's front face shows (it shrinks as the key goes down)
        constexpr float whiteLip = 7.0f, blackLip = 6.0f, selectedLip = 3.0f, pressedLip = 1.5f;
        constexpr float keyPad = 10.0f; // room around each cached key image for its shadow and glow

        juce::Path keyShape (juce::Rectangle<float> key, float corner)
        {
            juce::Path p;
            p.addRoundedRectangle (key.getX(), key.getY(), key.getWidth(), key.getHeight(), corner, corner, false, false, true, true);
            return p;
        }

        float lipFor (bool black, bool isSelected, bool isPressed)
        {
            return isPressed ? pressedLip : (isSelected ? selectedLip : (black ? blackLip : whiteLip));
        }

        void paintWhiteKey (juce::Graphics& g, juce::Rectangle<float> key, bool isSelected, bool isHover, bool isPressed)
        {
            using namespace pt1;
            const auto lip = lipFor (false, isSelected, isPressed);
            const auto down = isSelected || isPressed;

            // Contact shadow on the keybed, shorter when the key is down
            juce::DropShadow (light::shadow.withAlpha (down ? 0.18f : 0.3f), 4, { 1, down ? 1 : 2 }).drawForPath (g, keyShape (key, 3.0f));

            // Front lip: the key's front face, visible below the top surface
            g.setGradientFill (juce::ColourGradient::vertical (colours::ivoryLip, key.getBottom() - lip, colours::ivoryLip.darker (0.15f), key.getBottom()));
            g.fillPath (keyShape (key, 3.0f));

            // Top face: bright along the top edge, falling off towards the front; inverted and a
            // touch brighter when pushed in, so a down key doesn't just look dull
            const auto face = key.withTrimmedBottom (lip);
            auto top = colours::ivoryTop, bottom = colours::ivoryBottom;
            if (down)
                std::swap (top, bottom);
            g.setGradientFill (juce::ColourGradient::vertical (top.brighter (down ? 0.03f : 0.0f), face.getY(), bottom, face.getBottom()));
            g.fillPath (keyShape (face, 2.0f));

            if (isHover)
            {
                g.setColour (juce::Colours::white.withAlpha (0.35f));
                g.fillPath (keyShape (face, 2.0f));
            }

            if (isSelected)
            {
                // Colour wash, plus an inner glow line and a marker dot, so selection reads by shape too
                g.setGradientFill (juce::ColourGradient::vertical (colours::accent.withAlpha (0.28f), face.getY(), colours::accent.withAlpha (0.6f), face.getBottom()));
                g.fillPath (keyShape (face, 2.0f));
                g.setColour (colours::accent.withAlpha (0.9f));
                g.strokePath (keyShape (face.reduced (1.0f), 2.0f), juce::PathStrokeType (1.5f));
                const auto dot = face.getWidth() * 0.22f;
                g.setColour (colours::accentDark);
                g.fillEllipse (juce::Rectangle<float> (dot, dot).withCentre ({ face.getCentreX(), face.getBottom() - 24.0f }));
            }

            // Side walls: narrow darker strips imply the key's thickness
            g.setGradientFill (juce::ColourGradient::horizontal (juce::Colours::black.withAlpha (0.12f), face.getX(), juce::Colours::transparentBlack, face.getX() + 2.5f));
            g.fillRect (face.withWidth (2.5f));
            g.setGradientFill (juce::ColourGradient::horizontal (juce::Colours::transparentBlack, face.getRight() - 2.5f, juce::Colours::black.withAlpha (0.16f), face.getRight()));
            g.fillRect (face.withTrimmedLeft (face.getWidth() - 2.5f));

            // Edge between the top face and the front lip
            g.setColour (juce::Colours::black.withAlpha (0.12f));
            g.fillRect (key.getX() + 1.0f, face.getBottom() - 0.5f, key.getWidth() - 2.0f, 1.0f);
        }

        void paintBlackKey (juce::Graphics& g, juce::Rectangle<float> key, bool isSelected, bool isHover, bool isPressed)
        {
            using namespace pt1;
            const auto lip = lipFor (true, isSelected, isPressed);
            const auto down = isSelected || isPressed;
            const auto shape = keyShape (key, 2.5f);

            // Black keys sit above the whites and cast a stronger shadow onto them; a selected key
            // also glows onto its neighbours
            if (isSelected)
                juce::DropShadow (colours::accent.withAlpha (0.55f), 7, {}).drawForPath (g, shape);
            juce::DropShadow (light::shadow.withAlpha (down ? 0.35f : 0.5f), down ? 4 : 6, { 2, down ? 2 : 4 }).drawForPath (g, shape);

            const auto bodyTop = isSelected ? colours::accent : colours::ebonyTop;
            const auto bodyBottom = isSelected ? colours::accentDark : colours::ebonyBottom;

            // Lip and sloped sides
            g.setColour (isSelected ? colours::accentDark.darker (0.3f) : colours::ebonyLip);
            g.fillPath (shape);

            // Top face, inset so the sloped sides show
            const auto face = key.withTrimmedBottom (lip).reduced (1.5f, 0.0f).withTrimmedTop (0.5f);
            g.setGradientFill (juce::ColourGradient::vertical (down ? bodyBottom : bodyTop.brighter (isHover ? 0.25f : 0.0f), face.getY(),
                down ? bodyTop : bodyBottom.brighter (isHover ? 0.2f : 0.0f), face.getBottom()));
            g.fillPath (keyShape (face, 2.0f));

            // Glossy highlight strip on the side facing the light
            const auto gloss = face.withWidth (face.getWidth() * 0.35f).translated (face.getWidth() * 0.12f, 0.0f).withTrimmedBottom (face.getHeight() * 0.35f).reduced (0.0f, 2.0f);
            g.setGradientFill (juce::ColourGradient::vertical (juce::Colours::white.withAlpha (isSelected ? 0.3f : 0.18f), gloss.getY(),
                juce::Colours::transparentWhite, gloss.getBottom()));
            g.fillRoundedRectangle (gloss, gloss.getWidth() / 2.0f);

            if (isSelected)
            {
                const auto dot = face.getWidth() * 0.3f;
                g.setColour (colours::caseTop);
                g.fillEllipse (juce::Rectangle<float> (dot, dot).withCentre ({ face.getCentreX(), face.getBottom() - 30.0f })); // above the note label
            }
        }

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
            pressedNote = *note;
            setNote (*note, dragAdds);
            repaint();
        }
    }

    void ChordKeyboard::mouseDrag (const juce::MouseEvent& event)
    {
        if (const auto note = noteAt (event.position); note && *note != lastDraggedNote)
        {
            lastDraggedNote = *note;
            pressedNote = *note;
            setNote (*note, dragAdds);
            repaint();
        }
    }

    void ChordKeyboard::mouseUp (const juce::MouseEvent& event)
    {
        pressedNote = -1;
        setHovered (noteAt (event.position).value_or (-1));
        repaint();
    }

    void ChordKeyboard::mouseMove (const juce::MouseEvent& event)
    {
        setHovered (noteAt (event.position).value_or (-1));
    }

    void ChordKeyboard::mouseExit (const juce::MouseEvent&)
    {
        setHovered (-1);
    }

    void ChordKeyboard::setHovered (int note)
    {
        if (note != hoveredNote)
        {
            hoveredNote = note;
            repaint();
        }
    }

    void ChordKeyboard::resized()
    {
        art = {}; // key sizes changed
    }

    ChordKeyboard::KeyState ChordKeyboard::stateOf (int note) const
    {
        if (note == pressedNote)
            return pressed;
        const auto isSelected = shown[(size_t) note];
        const auto isHover = note == hoveredNote;
        return isSelected ? (isHover ? selectedHover : selected) : (isHover ? hover : idle);
    }

    const ChordKeyboard::KeyArt& ChordKeyboard::keyArt (float scale)
    {
        const auto whiteBounds = keyBounds (windowStart);
        const auto blackBounds = keyBounds (windowStart + 1);
        const auto whiteSize = juce::Point<float> (whiteBounds.getWidth(), whiteBounds.getHeight());
        const auto blackSize = juce::Point<float> (blackBounds.getWidth(), blackBounds.getHeight());

        if (juce::approximatelyEqual (art.scale, scale) && art.whiteSize == whiteSize && art.blackSize == blackSize)
            return art;

        art.scale = scale;
        art.whiteSize = whiteSize;
        art.blackSize = blackSize;

        for (int state = 0; state < numKeyStates; ++state)
        {
            const auto isSelected = state == selected || state == selectedHover;
            const auto isHover = state == hover || state == selectedHover;
            const auto isPressed = state == pressed;

            // Images cover the key's full slot plus padding; white keys sit 1 px in from each side,
            // leaving a hairline of keybed between neighbours
            const auto white = juce::Rectangle<float> (whiteSize.x, whiteSize.y);
            art.white[(size_t) state] = pt1::renderCached (white.expanded (keyPad), scale, [&] (juce::Graphics& g) {
                paintWhiteKey (g, white.reduced (1.0f, 0.0f), isSelected, isHover, isPressed);
            });

            const auto black = juce::Rectangle<float> (blackSize.x, blackSize.y);
            art.black[(size_t) state] = pt1::renderCached (black.expanded (keyPad), scale, [&] (juce::Graphics& g) {
                paintBlackKey (g, black, isSelected, isHover, isPressed);
            });
        }
        return art;
    }

    void ChordKeyboard::paint (juce::Graphics& g)
    {
        const auto bounds = getLocalBounds().toFloat();
        const auto& images = keyArt (pt1::physicalScale (g));

        // Keybed: the dark slot the keys sit in
        g.setColour (pt1::colours::keybed);
        g.fillRoundedRectangle (bounds, 3.0f);

        for (auto black : { false, true })
        {
            for (int note = windowStart; note < windowStart + numKeys; ++note)
            {
                if (isBlack (note) != black)
                    continue;

                const auto& image = (black ? images.black : images.white)[(size_t) stateOf (note)];
                const auto key = keyBounds (note);
                g.drawImage (image, key.expanded (keyPad));

                // Label C keys, and every selected key so the chord can be read off the keyboard
                const auto on = shown[(size_t) note];
                if (on || note % 12 == 0)
                {
                    const auto lip = lipFor (black, on, note == pressedNote);
                    const auto labelArea = key.withTrimmedBottom (lip).withTop (key.getBottom() - lip - (black ? 22.0f : 16.0f));
                    g.setColour (on ? (black ? pt1::colours::caseTop : pt1::colours::accentDark) : pt1::colours::inkDim);
                    g.setFont (pt1::textFont (black ? 8.5f : 10.0f, on));
                    g.drawText (juce::MidiMessage::getMidiNoteName (note, true, ! black, 3), labelArea, juce::Justification::centred, false);
                }
            }
        }

        // The case overhangs the back of the keys and shades them
        g.setGradientFill (juce::ColourGradient::vertical (juce::Colours::black.withAlpha (0.35f), bounds.getY(),
            juce::Colours::transparentBlack, bounds.getY() + 10.0f));
        g.fillRect (bounds.withHeight (10.0f));
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
            using namespace pt1;

            // A rubber button that stays pushed in while its slot plays
            auto bounds = getLocalBounds().toFloat().reduced (4.0f, 5.0f);
            drawRubberButton (g, bounds, down || active, highlighted);
            bounds.translate (0.0f, down || active ? 1.0f : 0.0f);

            // LED: lit when this slot is playing (the pushed-in shape carries the state too)
            const auto led = juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ bounds.getRight() - 11.0f, bounds.getY() + 11.0f });
            if (active)
                juce::DropShadow (colours::accent.withAlpha (0.8f), 6, {}).drawForPath (g, [&] { juce::Path p; p.addEllipse (led); return p; }());
            g.setColour (active ? colours::accent : colours::bezel.withAlpha (filled ? 0.55f : 0.25f));
            g.fillEllipse (led);
            g.setColour (juce::Colours::white.withAlpha (active ? 0.6f : 0.2f));
            g.fillEllipse (led.reduced (2.0f).translated (-0.7f, -0.7f));

            bounds.reduce (10.0f, 5.0f);
            g.setColour (colours::ink);
            g.setFont (printFont (13.0f));
            g.drawText (juce::String (index + 1), bounds.removeFromTop (bounds.getHeight() * 0.5f), juce::Justification::bottomLeft, false);

            g.setColour (filled ? colours::ink : colours::inkDim);
            g.setFont (filled ? textFont (11.0f, true) : printFont (9.5f));
            g.drawFittedText (filled ? notes : juce::String ("EMPTY"), bounds.toNearestInt(), juce::Justification::centredLeft, 1, 0.75f);
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
}
