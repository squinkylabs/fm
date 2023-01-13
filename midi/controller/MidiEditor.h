#pragma once

#include <memory>

#include "MidiEditorContext.h"

class MidiEditorContext;
class MidiSelectionModel;
class MidiSequencer;
class MidiSong;
class MidiTrack;

class MidiEditor {
public:
    MidiEditor(std::shared_ptr<MidiSequencer>);
    MidiEditor(const MidiEditor&) = delete;
    ~MidiEditor();

    /************** functions that move the cursor position ***********/

    void selectNextNote();
    void extendSelectionToNextNote();
    void selectPrevNote();
    void extendSelectionToPrevNote();

    void selectAll();

    enum Advance {
        Tick,
        GridUnit,
        Beat,  // Quarter note
        Measure,
        All  // start or end
    };

    /**
     * Move cursor by a logical unit.
     * @param type is logical unit
     * @param multiplier. may be negative.
     */
    void advanceCursor(Advance type, int multiplier);
    void advanceCursorToTime(float time, bool extendSelection);
    void changeCursorPitch(int semitones);

    MidiNoteEventPtr moveToTimeAndPitch(float time, float pitchCV);

    // These two should be deprecated. they are "old school"
    void selectAt(float time, float pitchCV, bool extendSelection);
    void toggleSelectionAt(float time, float pitchCV);

    /*********** functions that edit/change the notes **************/

    void changePitch(int semitones);
    void changeStartTime(bool ticks, int amount);
    void changeStartTime(const std::vector<float>& shifts);
    void changeDuration(bool ticks, int amount);
    void changeDuration(const std::vector<float>& shifts);
    void setDuration(float duration);

    /************* functions that add or remove notes ************/

    enum class Durations { Whole,
                           Half,
                           Quarter,
                           Eighth,
                           Sixteenth };

    /**
     * returns the amount it would advance.
     */
    float insertPresetNote(Durations, bool advanceAfter);
    float insertDefaultNote(bool advanceAfter, bool extendSelection);
    void deleteNote();

    void grabDefaultNote();

    /*************                                   ***************/
    // Editing start time / duration / pitch
    void setNoteEditorAttribute(MidiEditorContext::NoteAttribute);

    //************** cut / copy / paste ***************/
    void cut();
    void copy();
    void paste();

    void changeTrackLength();
    bool isLooped() const;
    void loop();

    void assertCursorInSelection();
    // select any note that is under the cursor
    void updateSelectionForCursor(bool extendCurrent);

    MidiNoteEventPtr getNoteUnderCursor();

    static float getDuration(Durations dur);

    /**
     * returns the amount of time the editor would advance if it inserted the default note
     */
    float getAdvanceTimeAfterNote();

private:
    /**
     * The sequencer we will act on.
     * use weak ptr to break ref circle
     */
    std::weak_ptr<MidiSequencer> m_seq;
    std::shared_ptr<MidiSequencer> seq() {
        // This assumes of course that m_seq still exists
        auto ret = m_seq.lock();
        assert(ret);
        return ret;
    }

    std::shared_ptr<const MidiSequencer> seq() const {
        // This assumes of course that m_seq still exists
        auto ret = m_seq.lock();
        assert(ret);
        return ret;
    }

    std::shared_ptr<MidiTrack> getTrack();

    // move the cursor, if necessary.
    void updateCursor();
    void setCursorToNote(MidiNoteEventPtr note);
    void setNewCursorPitch(float pitch, bool extendSelection);

    /**
     *Return the amount by which they would advance
     */
    float insertNoteHelper(Durations dur, bool moveCursorAfter);
    void insertNoteHelper2(float dur, bool moveCursorAfter);
    void insertNoteHelper3(float duration, float advanceAmount, bool extendSelection);

    void extendSelectionToCurrentNote();
    void deleteNoteSub(const char* name);
    std::pair<float, float> getDefaultNoteDurationAndAdvance();
};

using MidiEditorPtr = std::shared_ptr<MidiEditor>;
