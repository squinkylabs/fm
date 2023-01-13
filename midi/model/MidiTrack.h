#pragma once

#include <vector>
#include <map>
#include <memory>

#include "FilteredIterator.h"
#include "SqCommand.h"

#include "SqMidiEvent.h"

class MidiLock;
class MidiTrack;
class MidiLock;
using MidiTrackPtr = std::shared_ptr<MidiTrack>;
using MidiTrackConstPtr = std::shared_ptr<const MidiTrack>;

class MidiTrack
{
public:
    MidiTrack(std::shared_ptr<MidiLock>);

    /**
     * this ctor puts an end event at time zero
     */
    MidiTrack(std::shared_ptr<MidiLock>, bool);
    static MidiTrackPtr makeEmptyTrack( std::shared_ptr<MidiLock>);


    int size() const;
    void assertValid() const;

    void insertEvent(MidiEventPtr ev);
    void deleteEvent(const MidiEvent&);
    void insertEnd(MidiEvent::time_t time);

    float getLength() const;
    std::shared_ptr<MidiEndEvent> getEndEvent();
    std::shared_ptr<MidiNoteEvent> getFirstNote();
    std::shared_ptr<MidiNoteEvent> getSecondNote();
    std::shared_ptr<MidiNoteEvent> getLastNote();

    /**
     * Returns all events as a vector, so that they may be indexed.
     * Obviously this is rather slow (O(n)), so don't use it for editing.
     */
    std::vector<MidiEventPtr> _testGetVector() const;

    using container = std::multimap<MidiEvent::time_t, MidiEventPtr>;
    using iterator = container::iterator;
    using reverse_iterator = container::reverse_iterator;
    using const_iterator = container::const_iterator;
    using const_reverse_iterator = container::const_reverse_iterator;
    using iterator_pair = std::pair<const_iterator, const_iterator>;

    using note_iterator = filtered_iterator<MidiEvent, MidiTrack::const_iterator>;
    using note_iterator_pair = std::pair<note_iterator, note_iterator>;
    note_iterator_pair timeRangeNotes(MidiEvent::time_t start, MidiEvent::time_t end) const;

    /**
     * finds an event that satisfies == and returns a pointer to it
     */
    const_iterator findEventDeep(const MidiEvent&);
    const_iterator findEventPointer(MidiEventPtrC);

    /**
     * Find the first MidiNoteEvent that is at time 't' or past it
     */
    const_iterator seekToTimeNote(MidiEvent::time_t time);
    const_iterator seekToLastNote();

    void setLength(float newTrackLength);

   
    /**
     * Returns pair of iterators for all events  start <= t <= end
     */
    iterator_pair timeRange(MidiEvent::time_t start, MidiEvent::time_t end) const;

    iterator begin()
    {
        return events.begin();
    }
    iterator end()
    {
        return events.end();
    }
    const_iterator begin() const
    {
        return events.begin();
    }
    const_iterator end() const
    {
        return events.end();
    }

    void _dump() const;

    /**
     * factory method to generate test content.
     */
    enum class TestContent
    {
        eightQNotes,
        empty,
        oneNote123,                 // a single note at time 1.23, one bar
        oneQ1,                      // a single quarter note at time 1.0, pitch = 3.0
        oneQ1_75,                      // a single quarter note at time 1.0 pitch = 7.5
        FourTouchingQuarters,       // a measure with four full-duration quarters
        FourAlmostTouchingQuarters,
        FourAlmostTouchingQuarters_12,  // a measure with four almost full-duration quarters at 1.2 volts
        FourTouchingQuartersOct,    // a measure with four full-duration quarters, pitch 3,4,5,6
        eightQNotesCMaj             // two measures long
    };
    static MidiTrackPtr makeTest(TestContent, std::shared_ptr<MidiLock>);
    std::shared_ptr<MidiLock> lock;
private:
    container events;

    static MidiTrackPtr makeTest1(std::shared_ptr<MidiLock>);
    static MidiTrackPtr makeTestCmaj(std::shared_ptr<MidiLock>);
  //  static MidiTrackPtr makeTestEmpty(std::shared_ptr<MidiLock>);
    static MidiTrackPtr makeTestNote123(std::shared_ptr<MidiLock>);
    static MidiTrackPtr makeTestOneQ1(std::shared_ptr<MidiLock>, float pitch);
    static MidiTrackPtr makeTestFourTouchingQuarters(bool exactDuration, std::shared_ptr<MidiLock>, bool spacePitchByOctave, float pitch);
};

using MidiTrackPtr = std::shared_ptr<MidiTrack>;

