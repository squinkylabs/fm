#pragma once

#include "MidiTrack.h"
#include "FilteredIterator.h"
#include <memory>

class ISeqSettings;
class MidiSong;
class MidiSelectionModel;
class NoteScreenScale;

using MidiSongPtr = std::shared_ptr<MidiSong>;

#define _NEWTAB

class MidiEditorContext  : public std::enable_shared_from_this<MidiEditorContext>
{
public:

    // TODO: use this later?
    class Range
    {
    public:
        float pitchLow = 0;
        float pitchHigh = 0;
        float start = 0;
        float end = 0;
    };

    MidiEditorContext(std::shared_ptr<MidiSong>, std::shared_ptr<ISeqSettings>);
    ~MidiEditorContext();

    float cursorPitch() const
    {
        return m_cursorPitch;
    }

    /**
     * Sets cursor pitch in CV (1v/8) units
     */
    void setCursorPitch(float pitch)
    {
        assert(pitch <= 10);
        assert(pitch >= -10);
        m_cursorPitch = pitch;
    }
    float cursorTime() const
    {
        return m_cursorTime;
    }
    void setCursorTime(float time)
    {
        m_cursorTime = time;
    }

    MidiEvent::time_t startTime()
    {
        return m_startTime;
    }
    void setStartTime(MidiEvent::time_t t)
    {
        m_startTime = t;
    }
    void setEndTime(MidiEvent::time_t t)
    {
        m_endTime = t;
    }
    void setTimeRange(MidiEvent::time_t start, MidiEvent::time_t end)
    {
        m_startTime = start;
        m_endTime = end;
        assert(end > start);
    }

    MidiEvent::time_t endTime()
    {
        return m_endTime;
    }
    float pitchHigh()
    {
        return m_pitchHigh;
    }
    float pitchLow()
    {
        return m_pitchLow;
    }
    void setPitchLow(float p)
    {
        m_pitchLow = p;
    }
    void setPitchHi(float p)
    {
        m_pitchHigh = p;
    }
    void setPitchRange(float l, float h)
    {
        assert(h >= l);
        assert(h <= 10);
        m_pitchHigh = h;
        m_pitchLow = l;
    }
    int getTrackNumber()
    {
        return trackNumber;
    }
    void setTrackNumber(int n)
    {
        trackNumber = n;
    }

    MidiTrackPtr getTrack();

    void setScaler(std::shared_ptr<NoteScreenScale> _scaler);

    std::shared_ptr<NoteScreenScale> getScaler()
    {
        return scaler;
    }

    void setCursorToNote(MidiNoteEventPtrC note);
    void setCursorToSelection(std::shared_ptr<MidiSelectionModel> selection);

    // TODO: change to const_iterator
    using iterator = filtered_iterator<MidiEvent, MidiTrack::const_iterator>;
    using iterator_pair = std::pair<iterator, iterator>;

    /**
     * gets an iterator for all the events in the current edit context.
     * 
     * @param preMargin is how much earlier to start before the start of the edit context.
     */
    iterator_pair getEvents(float preMargin) const;
    iterator_pair getEvents(float timeLow, float timeHigh, float pitchLow, float pitchHigh) const;

    std::shared_ptr<MidiSong> getSong() const;

    void scrollVertically(float pitchCV);

    // Which field of note is being edited?
    enum class NoteAttribute
    {
        Pitch,
        Duration,
        StartTime
    };

    NoteAttribute noteAttribute = NoteAttribute::Pitch;

    void assertValid() const;

    bool cursorInViewport() const;
    void assertCursorInViewport() const;
    void scrollViewportToCursorPitch();
    bool cursorInViewportTime() const;
    void adjustViewportForCursor();

    std::shared_ptr<ISeqSettings> settings()
    {
        return _settings;
    }

     /**
     * If zero, take duration from grid.
     * If >=, contains duration
     */
    float insertNoteDuration = 0;

    void setNewSong(MidiSongPtr song);

#ifdef _NEWTAB
    /**
     * The cursor note is the note that is (will be)
     * at the edit cursor.
     */
    void setCursorNote(MidiNoteEventPtr);

    /**
     * Will get the cursor not if it still is in the track,
     * and still selected.
     */
    MidiNoteEventPtr getCursorNote(std::shared_ptr<MidiSelectionModel>);
    
#endif
    
private:
    float m_cursorTime = 0;
    float m_cursorPitch = 0;

    // range will include t == start time, but will not
    // include t == endTime
    MidiEvent::time_t m_startTime = 0;
    MidiEvent::time_t m_endTime = 1;

    // pitch is inclusive: Low and Hi will be included
    float m_pitchLow = 0;
    float m_pitchHigh = 0;

    int trackNumber = 0;

    std::shared_ptr<NoteScreenScale> scaler;

    // Use weak ref to break circular dependency
    std::weak_ptr<MidiSong> _song;

    std::shared_ptr<ISeqSettings> _settings;

#ifdef _NEWTAB
    std::weak_ptr<MidiNoteEvent> cursorNote;
#endif
};

using MidiEditorContextPtr = std::shared_ptr<MidiEditorContext>;