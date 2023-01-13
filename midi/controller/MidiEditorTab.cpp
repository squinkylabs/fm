/**
 * These are the parts of MidiEditor that deal with select next/prev
 */

#include "MidiEditor.h"
#include "MidiSequencer.h"

#if defined(_NEWTAB)

static MidiNoteEventPtr findNextNoteAtOrPastCursorInTime(MidiSequencerPtr seq);
static MidiNoteEventPtr findPrevNoteAtOrBeforeCursorInTime(MidiSequencerPtr seq);
static MidiNoteEventPtr findNextNoteInOrder(MidiSequencerPtr seq, MidiNoteEventPtr curNote);
static MidiNoteEventPtr findPrevNoteInOrder(MidiSequencerPtr seq, MidiNoteEventPtr curNote);
//static void addCursorNoteToSelection

void MidiEditor::selectNextNote() {
    MidiSequencerPtr sq = seq();
    sq->assertValid();

    MidiNoteEventPtr curNote = sq->context->getCursorNote(sq->selection);
    sq->selection->clear();

    MidiNoteEventPtr note;
    if (curNote) {
        note = findNextNoteInOrder(sq, curNote);
    } else {
        note = findNextNoteAtOrPastCursorInTime(sq);
    }

    if (note) {
        // add to selection
        sq->selection->addToSelection(note, true);
        assertEQ(sq->selection->size(), 1);
    }

    sq->context->setCursorNote(note);
    // now set cursor in context to this note.
    // note that updateCursor is an old func - may not be quite what we want now
    updateCursor();
    seq()->context->adjustViewportForCursor();
}

void MidiEditor::selectPrevNote() {
    MidiSequencerPtr sq = seq();
    sq->assertValid();
    MidiNoteEventPtr cursorNote = sq->context->getCursorNote(sq->selection);
    sq->selection->clear();

    MidiNoteEventPtr noteToSelect;

    // If cursor is in a note, and note at starts, then we should just move it to start
    {
        MidiNoteEventPtr candidateNote = getNoteUnderCursor();
        if (candidateNote) {
            if (sq->context->cursorTime() > candidateNote->startTime) {
                sq->context->setCursorTime(candidateNote->startTime);
                noteToSelect = candidateNote;
            }
        }
    }

    // If the special case, above, didn't kick in, then do normal prev
    if (!noteToSelect) {
        if (cursorNote) {
            noteToSelect = findPrevNoteInOrder(sq, cursorNote);
        } else {
            noteToSelect = findPrevNoteAtOrBeforeCursorInTime(sq);
        }
    }

    if (noteToSelect) {
        sq->selection->addToSelection(noteToSelect, true);
        assertEQ(sq->selection->size(), 1);
    }

    sq->context->setCursorNote(noteToSelect);
    // now set cursor in context to this note.
    // note that updateCursor is an old func - may not be quite what we want now
    updateCursor();
    seq()->context->adjustViewportForCursor();
}

void MidiEditor::extendSelectionToNextNote() {
    MidiSequencerPtr sq = seq();
    sq->assertValid();
    MidiNoteEventPtr curNote = sq->context->getCursorNote(sq->selection);

    MidiNoteEventPtr note;
    if (curNote) {
        note = findNextNoteInOrder(sq, curNote);
    } else {
        note = findNextNoteAtOrPastCursorInTime(sq);
    }

    if (note) {
        // add to selection
        sq->selection->addToSelection(note, true);
        // assertEQ(sq->selection->size(), 1);
    }

    sq->context->setCursorNote(note);
    // now set cursor in context to this note.
    // note that updateCursor is an old func - may not be quite what we want now

    setCursorToNote(note);
    seq()->context->adjustViewportForCursor();
}

void MidiEditor::extendSelectionToPrevNote() {
    MidiSequencerPtr sq = seq();
    sq->assertValid();
    MidiNoteEventPtr curNote = sq->context->getCursorNote(sq->selection);

    MidiNoteEventPtr note;
    if (curNote) {
        note = findPrevNoteInOrder(sq, curNote);
    } else {
        note = findPrevNoteAtOrBeforeCursorInTime(sq);
    }

    if (note) {
        // add to selection
        sq->selection->addToSelection(note, true);
        // assertEQ(sq->selection->size(), 1);
    }

    sq->context->setCursorNote(note);
    // now set cursor in context to this note.
    // note that updateCursor is an old func - may not be quite what we want now

    // updateCursor(); (not right
    setCursorToNote(note);
    seq()->context->adjustViewportForCursor();
}

static MidiNoteEventPtr findNextNoteInOrder(MidiSequencerPtr seq, MidiNoteEventPtr curNote) {
    const auto track = seq->context->getTrack();
    assert(curNote);

    MidiTrack::const_iterator it = track->findEventPointer(curNote);
    assert(it != track->end());

    ++it;  // next event
    if (it == track->end()) {
        return curNote;  // don't go past last note, just stick on it
    }

    MidiEventPtr evt = it->second;

    // If it's an end event, it will case to null
    MidiNoteEventPtr note = safe_cast<MidiNoteEvent>(evt);
    return note ? note : curNote;
}

static MidiNoteEventPtr findPrevNoteInOrder(MidiSequencerPtr seq, MidiNoteEventPtr curNote) {
    const auto track = seq->context->getTrack();
#ifdef _LOG
    track->_dump();
#endif

    assert(curNote);

    MidiTrack::const_iterator it = track->findEventPointer(curNote);
    assert(it != track->end());

    if (it == track->begin()) {
        return curNote;  // stick on first one, it no others before
    }

    --it;  // prev event
    MidiEventPtr evt = it->second;

    // If it's an end event, it will case to null
    MidiNoteEventPtr note = safe_cast<MidiNoteEvent>(evt);
    assert(note);
    return note;
    // return note ? note : curNote;
}

static MidiNoteEventPtr findNextNoteAtOrPastCursorInTime(MidiSequencerPtr seq) {
    const auto t = seq->context->cursorTime();
    const auto track = seq->context->getTrack();
#ifdef _LOG
    printf("in selectNextNotePastCursor t=%.2f\n", t);
#endif

    // first, seek into track until cursor time.
    MidiTrack::const_iterator it = track->seekToTimeNote(t);
    if (it == track->end()) {
#ifdef _LOG
        printf("seeked past end\n");
#endif

        return nullptr;
    }
    MidiNoteEventPtr note = safe_cast<MidiNoteEvent>(it->second);
    return note;
}

static MidiNoteEventPtr findPrevNoteAtOrBeforeCursorInTime(MidiSequencerPtr seq) {
    const auto t = seq->context->cursorTime();
    const auto track = seq->context->getTrack();

    // first, seek into track until cursor time.
    MidiTrack::const_iterator it = track->seekToTimeNote(t);

    if (it == track->end()) {
        it = track->seekToLastNote();
        if (it == track->end()) {
            return nullptr;
        }
    }

    while (it->first > t) {
        if (it == track->begin()) {
            return nullptr;
        } else {
            --it;
        }
    }

    assert(it->first <= t);

    MidiNoteEventPtr note = safe_cast<MidiNoteEvent>(it->second);
    return note;

#if 0
    // if it came back with a note exactly at cursor time,
    // check if it's acceptable.
    if ((it->first < t) || (it->first == t && atCursorOk)) {
        MidiNoteEventPtr note = safe_cast<MidiNoteEvent>(it->second);
        if (note) {
            seq->selection->addToSelection(note, keepExisting);
            return note;
        }
    }

    MidiTrack::const_iterator bestSoFar = it;

    // now either this previous is acceptable, or something before it
    while (true) {
        MidiNoteEventPtr note = safe_cast<MidiNoteEvent>(it->second);
        if (note && (note->startTime < t)) {
            seq->selection->addToSelection(note, keepExisting);
            return note;
        }
        if (it == track->begin()) {
            break;  // give up if we are at start and have found nothing good
        }
        --it;       // if nothing good, try previous
    }

    // If nothing past where we are, it's OK, even if it is at the same time
    MidiNoteEventPtr note = safe_cast<MidiNoteEvent>(bestSoFar->second);
    if (note && note->startTime >= t) {
        note = nullptr;
    }
    if (note) {
        seq->selection->select(bestSoFar->second);
    }
    return note;
#endif
}

#endif

//*********************************************************************************************************
#if !defined(_NEWTAB)

static MidiNoteEventPtr selectNextNotePastCursor(bool atCursorOk,
                                                 bool keepExisting,
                                                 MidiSequencerPtr seq);
static MidiNoteEventPtr selectPrevNoteBeforeCursor(bool atCursorOk,
                                                   bool keepExisting,
                                                   MidiSequencerPtr seq);

/**
 * Here's the "new" algorithm:
 *  if there is a note selected, search for the next note that's later
 *  in the track than the current selection. If not found,
 *  don't change anything.
 *
 *  If nothing selected, then search for the first note that is later or at the same time as the cursor.
 *
 * For now we can base everything from cursor. Later, when we do multi-select, will need to be smarter.
 */

void MidiEditor::selectNextNote() {
#ifdef _LOG
    printf("select next note #sel=%d\n", seq()->selection->size());
#endif
    seq()->assertValid();

    MidiTrackPtr track = getTrack();
    assert(track);

    // If nothing is selected, we will accept a note right at cursor. Don't know how we get to that state,
    // but the thinking is that if we are on a not and it isn't selected, the selectin it is progress.
    const bool acceptCursorTime = seq()->selection->empty();

    // now let's try clearing out all the selection that we want to drop.
    seq()->selection->clear();

#ifdef _LOG
    printf("in selecteNextNote, acceptCursorTime=%d, numsel=%d curTime = %.2f\n",
           acceptCursorTime,
           seq()->selection->size(),
           seq()->context->cursorTime());
#endif

    selectNextNotePastCursor(acceptCursorTime, false, seq());

    updateCursor();
    seq()->context->adjustViewportForCursor();

#ifdef _LOG
    printf("leave select next note\n");
    fflush(stdout);
#endif
}

void MidiEditor::extendSelectionToNextNote() {
    seq()->assertValid();

    MidiTrackPtr track = getTrack();
    assert(track);
    const bool acceptCursorTime = seq()->selection->empty();
    MidiNoteEventPtr note = selectNextNotePastCursor(acceptCursorTime, true, seq());

    // move cursor to newly selected note - it if exists
    if (note) {
        setCursorToNote(note);
    } else {
        updateCursor();
    }
    seq()->context->adjustViewportForCursor();
}

void MidiEditor::selectPrevNote() {
    seq()->assertValid();
    MidiTrackPtr track = getTrack();
    assert(track);
    const bool acceptCursorTime = seq()->selection->empty();
    selectPrevNoteBeforeCursor(acceptCursorTime, false, seq());
    updateCursor();
    seq()->context->adjustViewportForCursor();
}

void MidiEditor::extendSelectionToPrevNote() {
    seq()->assertValid();

    MidiTrackPtr track = getTrack();
    assert(track);
    const bool acceptCursorTime = seq()->selection->empty();
    MidiNoteEventPtr note = selectPrevNoteBeforeCursor(acceptCursorTime, true, seq());

    // move cursor to newly selected note - it if exists
    if (note) {
        setCursorToNote(note);
    } else {
        updateCursor();
    }
    seq()->context->adjustViewportForCursor();
}

/**
 * returns the not that was added to selection, or nullptr if none
 */
static MidiNoteEventPtr selectNextNotePastCursor(bool atCursorOk,
                                                 bool keepExisting,
                                                 MidiSequencerPtr seq) {
    const auto t = seq->context->cursorTime();
    const auto track = seq->context->getTrack();
#ifdef _LOG
    printf("in selectNextNotePastCursor t=%.2f\n", t);
#endif

    // first, seek into track until cursor time.
    MidiTrack::const_iterator it = track->seekToTimeNote(t);
    if (it == track->end()) {
#ifdef _LOG
        printf("seeked past end\n");
#endif
        if (!keepExisting) {
            seq->selection->clear();
        }
        return nullptr;
    }

    // if it came back with a note exactly at cursor time,
    // check if it's acceptable.
    if ((it->first > t) || (it->first == t && atCursorOk)) {
        MidiNoteEventPtr note = safe_cast<MidiNoteEvent>(it->second);
        if (note) {
            seq->selection->addToSelection(note, keepExisting);
#ifdef _LOG
            printf("selectNextNotePastCursor added first note to selection at time %.2f #sel=%d\n",
                   note->startTime,
                   seq->selection->size());
#endif
            return note;
        }
    }

    MidiTrack::const_iterator bestSoFar = it;

    // If must be past cursor time, advance to next
    ++it;

    for (; it != track->end(); ++it) {
        MidiNoteEventPtr note = safe_cast<MidiNoteEvent>(it->second);
        if (note) {
            seq->selection->addToSelection(note, keepExisting);
#ifdef _LOG
            printf("selectNextNotePastCursor in search found one at %.2f, #sel=%d\n", note->startTime, seq->selection->size());
#endif
            return note;
        }
    }

    // If nothing past where we are, it's ok, even if it is at the same time
    MidiNoteEventPtr note = safe_cast<MidiNoteEvent>(bestSoFar->second);
    if (note) {
        seq->selection->addToSelection(note, keepExisting);
#ifdef _LOG
        printf("selectNextNotePastCursoraccept last one at %.2f #sel=%d\n", note->startTime, seq->selection->size());
#endif
    }
    return note;
}

static MidiNoteEventPtr selectPrevNoteBeforeCursor(bool atCursorOk,
                                                   bool keepExisting,
                                                   MidiSequencerPtr seq) {
    const auto t = seq->context->cursorTime();
    const auto track = seq->context->getTrack();

    // first, seek into track until cursor time.
    MidiTrack::const_iterator it = track->seekToTimeNote(t);
    if (it == track->end()) {
        it = track->seekToLastNote();
        if (it == track->end()) {
            seq->selection->clear();
            return nullptr;
        }
    }

    // if it came back with a note exactly at cursor time,
    // check if it's acceptable.
    if ((it->first < t) || (it->first == t && atCursorOk)) {
        MidiNoteEventPtr note = safe_cast<MidiNoteEvent>(it->second);
        if (note) {
            seq->selection->addToSelection(note, keepExisting);
            return note;
        }
    }

    MidiTrack::const_iterator bestSoFar = it;

    // now either this previous is acceptable, or something before it
    while (true) {
        MidiNoteEventPtr note = safe_cast<MidiNoteEvent>(it->second);
        if (note && (note->startTime < t)) {
            seq->selection->addToSelection(note, keepExisting);
            return note;
        }
        if (it == track->begin()) {
            break;  // give up if we are at start and have found nothing good
        }
        --it;  // if nothing good, try previous
    }

    // If nothing past where we are, it's OK, even if it is at the same time
    MidiNoteEventPtr note = safe_cast<MidiNoteEvent>(bestSoFar->second);
    if (note && note->startTime >= t) {
        note = nullptr;
    }
    if (note) {
        seq->selection->select(bestSoFar->second);
    }
    return note;
}
#endif
